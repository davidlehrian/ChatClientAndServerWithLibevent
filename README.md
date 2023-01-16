# Simple Chat Server And Client
Example chat server using libevent and simple client using poll.

This example is a combination of what I learned from [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/) and [A tiny introduction to asynchronous IO](https://libevent.org/libevent-book/01_intro.html). 

I did the original development on a Raspberry Pi 4 and then ported it to Windows using [Cygwin](https://www.cygwin.com/). I did the development using MS Visual Studio Code and I included the .vscode folders for both systems. The entries in these JSON files may need to be edited for your specific configuration which I will describe more fully below. 

## Linux Debian/Raspberry Pi OS Build<br>
As the network programming examples from [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/) are written for Linux I did the intial work on the Raspberry Pi. I have the Raspberry Pi OS Bullseye installed and up to date and it comes with Microsoft Visual Studio Code so this seemed like a reasonable environment with which to proceed.

To develop in MS Code using C/C++ you need to click on the "Extensions" button (Crtl-Shift-X) and install:<br>
>Microsoft C/C++<br>
>Microsoft C/C++ Extension Pack<br>
>Microsoft C/C++ Themes (I think this was was automagically installed when I installed one of the other two)<br>

Then go to File->Open Folder and open the folder containing this code.

You won't be able to build just yet because you first need to download, build and install libevent. Fortunately this is pretty straigtforward. The only prerequisite is that you have openssl-dev installed. Open a shell terminal and type:
>sudo apt-get install libssl-dev 

Then download [libevent-2.1.12-stable.tar.gz](https://github.com/libevent/libevent/releases/download/release-2.1.12-stable/libevent-2.1.12-stable.tar.gz), extract it to a folder and change directory to this folder and type the following. 
>$ ./configure --prefix=/opt/libevent<br>
>$ make<br>
>$ sudo make install<br>

This will install libevent into /opt/libevent which is where the VS Code configuration files are expecting to find it so you won't have to edit them. Now you can build the example code in VS Code by opening each of the C files and pressing Crtl-Shift-B which builds the currently active file. 

To run them in the debugger make "libeventchatserver.c" the current file, click the "Run and Debug" button (Crtl-Shift-D), and at the top select the Libevent Chat Server from the drop down and click the green launch icon to the left of the selected configuration. Once the server is running you can then make "chatclient.c" the current file, select the Chat Client configuration and launch it twice. You can also launch these from a shell terminal. Now anything you type into the Chat Client terminal you should see in all the other chat terminals. 

### Note on .vscode files.<br>
`c_cpp_properties.json: file where you specify to VS Code where to find headers. This is for intellisense only.`<br>
`tasks.json:            file where you specify the additional headers for the compliler and the libraries for the linker.`<br>
`launch.json:           file where you specify the launch configurations for running the debugger.`<br>

## MS Windows Build<br>
I used Microsoft Visual Studio Code for development in Windows as well. You need to install the same "Extensions" as described in the Linux Debian/Raspberry Pi OS Build instructions. 

In the file folder you need to delete .vscode and rename .vscode-win to .vscode. 

In order to compile on Windows I used [Cygwin](https://www.cygwin.com/install.html) to get the required libevent and openssl packages. When you install Cygwin for the first time it will install the "minimal base packages", but in addition to the base packages you need to select View "Full" from the drop down, search for "libevent" and install the latest versions of "libevent-devel" and "libevent2.1_7". If you install Cygwin to c:\cygwin you will not have to change the VS Code configuration files otherwise they will need to be updated to point to your install location.

Building and running the examples is now the same as described in the Linux Debian/Raspberry Pi OS Build instructions. However, I will note that I had a more difficult time getting the server to run in the debugger and to get the client to connect to it. I had my best success but opening a command prompt and running the server from the command line and then connecting with the client. If it didn't connect I then opened a second command prompt and tried it and it would usually work. If you press Ctrl-C to exit the server I have found you need to open a new command prompt in order to get it to connect. Not sure why, chalk it up to Windows wierdness. 

### Note on .vscode files for Windows.<br>
The files provide the same configuration information to VS Code as they do on Linux, but I noticed after specifying the header files in the c_cpp_properties.json on Windows, not only did intellisense find the headers, but gcc also found them as well so they don't need to be added to tasks.json. The only thing that does need to get specified in tasks.json is the additional libevent library for the linker.<br>
