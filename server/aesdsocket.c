#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#define AESD_SOCK_PORT "9000"
#define LISTEN_BACKLOG 10
#define WRITE_FILE "/var/tmp/aesdsocketdata"
#define READ_FILE WRITE_FILE
#define INITIAL_BUFFER_SIZE 512

int running = 1; //global flag to shutdown when SIGINT or SIGTERM is received 

void signal_handler(int signal){
    if(signal == SIGINT || signal == SIGTERM){
        syslog(LOG_INFO, "Caught signal, exiting");
        running = 0;
    }
}

int main(int argc, char *argv[]){
    //Start a logger
	openlog("Assignment5", 0, LOG_USER);

    //setup signal handler
    struct sigaction signal_action;
    memset(&signal_action, 0, sizeof(signal_action)); //make sure signal action is empty
    signal_action.sa_handler = signal_handler;
    sigaction(SIGINT, &signal_action, NULL);
    sigaction(SIGTERM, &signal_action, NULL);
    
    int status, sock_fd, their_fd, writefile_fd, readfile_fd;
    int newline = 0;
    struct addrinfo hints;
    struct addrinfo *servinfo;
    struct sockaddr_storage their_addr;
    char ip_string[INET6_ADDRSTRLEN];
    socklen_t addr_size;
    char *buffer=malloc(INITIAL_BUFFER_SIZE);
    if (buffer == NULL){
        syslog(LOG_ERR, "Unable to allocate memory for buffer.");
        exit(1);
    }
    
    ssize_t received, read_count;
    size_t buf_size = INITIAL_BUFFER_SIZE; //initial buffer size
    size_t total_received = 0; //number of bytes received into buffer

    memset(&hints, 0, sizeof(hints));   //make sure the hints struct is empty
    hints.ai_family = AF_INET;          //IPv4
    hints.ai_socktype = SOCK_STREAM;    //TCP Stream Socket
    hints.ai_flags = AI_PASSIVE;        //fill in the IP for me


    status = getaddrinfo(NULL, AESD_SOCK_PORT, &hints, &servinfo);

    if (status != 0){ //NULL defaults back to AI_PASSIVE, which sets it to localhost.  Port is 9000
        syslog(LOG_ERR, "GAI error: %s\n", gai_strerror(status));
        exit(2);
    }

    sock_fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);   //since this is a simple local example, we'll just take the first returned value
                                                                                 //If you did getaddrinfo on a hostname you may get multiple IP addr to loop through

    if (sock_fd == -1) {
        syslog(LOG_ERR, "socket error:%s\n", strerror(errno));
        exit(3);
    }

    int optval = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    status = bind(sock_fd, servinfo->ai_addr, servinfo->ai_addrlen);

    if (status == -1) {
        syslog(LOG_ERR, "bind error:%s\n", strerror(errno));
        exit(4);
    }


    //After binding, but before moving on, check if argument specified for daemon and start daemon if so
    if( (argc == 2) && (strcmp(argv[1], "-d") == 0) ){
        pid_t pid = fork();

        if(pid==-1){
            syslog(LOG_ERR, "fork error: %s", strerror(errno));
            exit(10);
        }

        if(pid > 0){
            //this is the parent thread, exit and let the child run as a daemon
            exit(0);
        }

        //the child continues as a daemon
        setsid(); //creates a new session to detach from terminal
    }



    status = listen(sock_fd, LISTEN_BACKLOG);

    if (status == -1) {
        syslog(LOG_ERR, "listen error:%s\n", strerror(errno));
        exit(5);
    }

    addr_size = sizeof(their_addr);
    
    while(running){
        their_fd = accept(sock_fd, (struct sockaddr *)&their_addr, &addr_size);
        //addr_size is a pointer because it takes size of sockaddr_storage as an input and outputs actual size of the addr filled in
        //sockaddr_storage is a struct defined in socket.h, used to store incoming client's address of unknown type (and size) and 
            //guaranteed to be castable to sockaddr

        if (their_fd == -1) { //if there is an error in accept other that signal interrupt, exit program with error
            if(errno == EINTR) break;
            else{
                syslog(LOG_ERR, "accept error:%s\n", strerror(errno));
                exit(6);
            }
        }

        //log successful connection
        getnameinfo((struct sockaddr *)&their_addr, addr_size, ip_string, sizeof(ip_string), NULL, 0, NI_NUMERICHOST);
        syslog(LOG_INFO, "Accepted connection from %s", ip_string);

        total_received = 0; //number of bytes received into buffer
        newline = 0;
        while(!newline){
            received = recv(their_fd, buffer + total_received, buf_size - total_received, 0);
            total_received += received;
            
            syslog(LOG_DEBUG, "total received: %zu", total_received);

            if (received == -1) {
                syslog(LOG_ERR, "receive error:%s\n", strerror(errno));
                exit(7);
            }

            //check for newline
            for (int i = total_received - received; i<total_received; i++){
                if(buffer[i] == '\n'){
                    newline = 1;
                    break;
                }
            }

            //no newline yet, grow the buffer
            if (!newline){
                buf_size *= 2;
                char *new_buf = realloc(buffer, buf_size);
                if (new_buf == NULL){ //makes sure realloc succeeds, and frees buffer if it doesn't to prevent memory leak
                    free(buffer); 
                    syslog(LOG_ERR, "Error growing buffer");
                    exit(8);
                }
                buffer = new_buf;
            }
            
            //syslog(LOG_DEBUG, "buffer %s", buffer);

            if(received == 0) //if the client closed the connection, stop receiving 
                break;

        }

        writefile_fd = open(WRITE_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
        write(writefile_fd, buffer, total_received);
        close(writefile_fd);
        readfile_fd = open(READ_FILE, O_RDONLY);

        do{
            read_count = read(readfile_fd, buffer, buf_size);
            
            if (read_count == -1) {
                syslog(LOG_ERR, "read file error:%s\n", strerror(errno));
                exit(9);
            }

            if(read_count)
                send(their_fd, buffer, read_count, 0);

        } while (read_count);

        close(readfile_fd);
        close(their_fd);
        syslog(LOG_INFO, "Closed connection from %s", ip_string);
    }

    remove(WRITE_FILE);
    close(sock_fd);
    free(buffer);
    freeaddrinfo(servinfo);
    return 0;
}
