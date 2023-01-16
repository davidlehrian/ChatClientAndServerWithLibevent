/**************************************************************************
SimpleChatServerAndClient is available for use under the following license, 
commonly known as the 3-clause (or "modified") BSD license:

==============================
Copyright (c) 2023 David Lehrian <david@lehrian.com>

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
==============================
**************************************************************************/

#include <sys/socket.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

#define MAX_LINE 512
#define PORT "8584" 

int bev_count = 0;
int bev_size = 10;
struct bufferevent **bevs;

// Return a listening socket
int get_listener_socket(void) {
    int listener;     // Listening socket descriptor
    int yes=1;        // For setsockopt() SO_REUSEADDR, below
    int rv;
    struct addrinfo hints, *ai, *p;
    // Get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; //AF_UNSPEC AF_INET or AF_INET6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }
    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) { 
            continue;
        }
        
        // Lose the pesky "address already in use" error message
#ifndef WIN32
            setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
#endif

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        int port;
        if (p->ai_family == AF_INET) {
            struct sockaddr_in *ipv4 = (struct sockaddr_in*)p->ai_addr;
            port = ipv4->sin_port;
        }else{
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)p->ai_addr;
            port = ipv6->sin6_port;   
        }
        fprintf(stdout,"starting on port %d.\n",ntohs(port));
    
        break;
    }
    // If we got here, it means we didn't get bound
    if (p == NULL) {
        return -1;
    }
    
    // free ai before we return
    freeaddrinfo(ai); 

    // Listen
    if (listen(listener, 10) == -1) {
        return -1;
    }

    return listener;
}


// Add a new bufferevent to the set
void add_to_bevs(struct bufferevent ***lbevs, 
    struct bufferevent *newbev,
    int *lbev_count, int *lbev_size) {
    // If we don't have room, add more space in the bevs array
    if (*lbev_count == *lbev_size) {
        *lbev_size *= 2;
        *lbevs = realloc(*lbevs, sizeof **lbevs * (*lbev_size));
    }
    (*lbevs)[*lbev_count] = newbev;
    (*lbev_count)++;
}

// Remove a bufferevent from the set
void del_from_bevs(struct bufferevent **bevs, int i, int *bev_count) {
    // Copy the one from the end over this one
    bevs[i] = bevs[*bev_count-1];
    (*bev_count)--;
}

void readcb(struct bufferevent *bev, void *ctx) {
    struct evbuffer *input, *output;
    char *line;
    size_t n;
    input = bufferevent_get_input(bev);
    output = bufferevent_get_output(bev);
    while ((line = evbuffer_readln(input, &n, EVBUFFER_EOL_LF))) {
        for (int i = 0; i < bev_count; i++){
            if (bevs[i] != bev){
                evbuffer_add(bufferevent_get_output(bevs[i]), line, n);
                evbuffer_add(bufferevent_get_output(bevs[i]), "\n", 1);
            }
        }
        free(line);
    }

    if (evbuffer_get_length(input) >= MAX_LINE) {
        /* Too long; just process what there is and go on so that the buffer
         * doesn't grow infinitely long. */
        char buf[1024];
        while (evbuffer_get_length(input)) {
            int n = evbuffer_remove(input, buf, sizeof(buf));
            for (int i = 0; i < bev_count; i++){
                if (bevs[i] != bev){
                    evbuffer_add(bufferevent_get_output(bevs[i]), buf, n);
                }
            }
        }
    }
}

void errorcb(struct bufferevent *bev, short error, void *ctx) {
    if (error & BEV_EVENT_EOF) {
        /* connection has been closed, do any clean up here */
        for (int i = 0; i < bev_count; i++){
            if (bevs[i] == bev){
                del_from_bevs(bevs,i,&bev_count);
                break;
            }
        }
    } else if (error & BEV_EVENT_ERROR) {
        /* check errno to see what error occurred */
        /* ... */
    } else if (error & BEV_EVENT_TIMEOUT) {
        /* must be a timeout event handle, handle it */
        /* ... */
    }
    bufferevent_free(bev);
}

void do_accept(evutil_socket_t listener, short event, void *arg) {
    struct event_base *base = arg;
    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);
    int fd = accept(listener, (struct sockaddr*)&ss, &slen);
    if (fd < 0) {
        perror("accept");
    } else if (fd > FD_SETSIZE) {
        close(fd);
    } else {
        struct bufferevent *bev;
        evutil_make_socket_nonblocking(fd);
        bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
        bufferevent_setcb(bev, readcb, NULL, errorcb, NULL);
        bufferevent_setwatermark(bev, EV_READ, 0, MAX_LINE);
        bufferevent_enable(bev, EV_READ|EV_WRITE);
        add_to_bevs(&bevs,bev,&bev_count,&bev_size);
    }
}


void run(void) {
    evutil_socket_t listener;
    struct event_base *base;
    struct event *listener_event;
    bevs = malloc(sizeof *bevs * bev_size);
    listener = get_listener_socket();
    base = event_base_new();
    if (!base) return; /*XXXerr*/
    listener_event = event_new(base, listener, EV_READ|EV_PERSIST, do_accept, (void*)base);
    /*XXX check it */
    event_add(listener_event, NULL);
    event_base_dispatch(base);
}

int main(int c, char **v) {
    printf("libevent chat server ");
    setvbuf(stdout, NULL, _IONBF, 0);
    run();
    return 0;
}
