/**************************************************************************
SimpleChatServerAndClient is available for use under the following license, 
commonly known as the 3-clause (or "modified") BSD license:

===========================================================================
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
**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <poll.h>
#include <arpa/inet.h>

#define PORT "8584"

int get_socket(char *serverName){
    int sockfd;
    int rv;
    int yes=1;
    struct addrinfo hints, *ai;
    memset(&hints,0,sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (rv = getaddrinfo(serverName,PORT,&hints,&ai) != 0){
        // there was an error
        fprintf(stderr, "error getting addrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    sockfd = socket(ai->ai_family,ai->ai_socktype,ai->ai_protocol);

    if (sockfd < 0){
        // there was an error
        fprintf(stderr, "error getting socket: %d\n", errno);
        exit(1);
    }

    if (connect(sockfd, ai->ai_addr, ai->ai_addrlen) == -1) {
        fprintf(stderr, "error connecting: %d\n", errno);
        close(sockfd);
        exit(1);
    }
    
    char ip[INET6_ADDRSTRLEN];
    int port;
    void *addr;
    if (ai->ai_family == AF_INET) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in*)ai->ai_addr;
        addr = &(ipv4->sin_addr);
        port = ipv4->sin_port;
    }else{
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)ai->ai_addr;
        addr = &(ipv6->sin6_addr);
        port = ipv6->sin6_port;
        
    }
    inet_ntop(ai->ai_family, addr, ip, sizeof ip);
    fprintf(stdout,"Connected to Server %s on port %d.\n",ip,ntohs(port));
    
    freeaddrinfo(ai);
    return sockfd;
}

int sendall(int s, char *buf, ssize_t *len) {
    int total = 0; // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;
    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }
    *len = total; // return number actually sent here
    return n==-1?-1:0; // return -1 on failure, 0 on success
}

int main(int argc, char *argv[]){
    int sockfd;
    char buf[512];
    
    if (argc != 2) {
	    fprintf(stderr,"usage: chatclient hostname\n");
	    exit(1);
	}

    sockfd = get_socket(argv[1]);
    fprintf(stdout,"Enter text to chat.\n");

	// Poll stdin and readfd for incoming data (ready-to-read)
	struct pollfd fds[2];

	fds[0].fd = 0;
	fds[0].events = POLLIN;

	fds[1].fd = sockfd;
	fds[1].events = POLLIN;

    while(1){
        int poll_count = poll(fds,2,-1);
        if (poll_count == -1){
            fprintf(stderr,"poll error\n");
            exit(1);
        }
        for (int i = 0; i < 2; i++){
            if (fds[i].revents & POLLIN){
                // if the fd != 0 this is a socket so read it
                // and write it to stdout
                if (fds[i].fd != 0){
                    int nbytes = recv(fds[i].fd, buf, sizeof buf, 0);
                    if (nbytes <= 0) {
                        if (nbytes == 0){
                        // Got error or connection closed by client
                            printf("libeventserver: socket %d hung up\n", sockfd);
                        } else {
                            perror("recv");
                        }
                        close(fds[i].fd); // Bye!
                        exit(1);
                    } else {
                        write(0,buf,nbytes);
                    }
                }else{
                    // else it is stdin so read it and sendall to the socket
                    char *line = NULL;
                    size_t len = 0;
                    ssize_t lineSize = 0;
                    lineSize = getline(&line, &len, stdin);
                    // drop cr
                    if (sendall(sockfd,line,&lineSize) == -1) {
                        perror("send");
                    }
                    free(line);
                }
            }
        }
    }
    return 0;
}
