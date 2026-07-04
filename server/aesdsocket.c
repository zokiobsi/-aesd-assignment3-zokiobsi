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
#include <sys/queue.h>
#include <pthread.h>

#define AESD_SOCK_PORT "9000"
#define LISTEN_BACKLOG 10
#define WRITE_FILE "/var/tmp/aesdsocketdata"
#define READ_FILE WRITE_FILE
#define INITIAL_BUFFER_SIZE 512

int running = 1; //global flag to shutdown when SIGINT or SIGTERM is received 
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t file_write_mutex = PTHREAD_MUTEX_INITIALIZER;
SLIST_HEAD(connection_list, connection_node) head = SLIST_HEAD_INITIALIZER(head);

//connection node structure
typedef struct connection_node {
    int client_fd;
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    int is_complete;
    pthread_t thread_id;
    SLIST_ENTRY(connection_node) entries;
} connection_node_t;


void signal_handler(int signal){
    if(signal == SIGINT || signal == SIGTERM){
        running = 0;
    }
}

void *timer_handler(void *args){
    while (running){
        for (int i = 0; i < 10 && running; i++){
            sleep(1);
        }
        if (!running) break;

        time_t now = time(NULL); //gets raw linux timestamp
        struct tm *timestamp_struct = localtime(&now);

        char timestamp[100];
        strftime(timestamp, sizeof(timestamp), "timestamp:%a, %d %b %Y %H:%M:%S %z\n", timestamp_struct);

        pthread_mutex_lock(&file_write_mutex);
        int fd = open(WRITE_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd != -1){
            write(fd, timestamp, strlen(timestamp));
            close(fd);
        }
        pthread_mutex_unlock(&file_write_mutex);
    }
    return NULL;
}

void *connection_handler(void *args){
    ssize_t received, read_count;
    size_t buf_size = INITIAL_BUFFER_SIZE; //initial buffer size
    size_t total_received = 0; //number of bytes received into buffer
    int writefile_fd, readfile_fd;
    int newline = 0;
    char ip_string[INET6_ADDRSTRLEN];

    connection_node_t *connection = (connection_node_t *)args;
    
    char *buffer=malloc(INITIAL_BUFFER_SIZE);
    if (buffer == NULL){
        syslog(LOG_ERR, "Unable to allocate memory for buffer.");
        connection->is_complete = 1;
        return NULL;
    }

    //log successful connection
        getnameinfo((struct sockaddr *)&connection->their_addr, connection->addr_size, ip_string, sizeof(ip_string), NULL, 0, NI_NUMERICHOST);
        syslog(LOG_INFO, "Accepted connection from %s", ip_string);

        total_received = 0; //number of bytes received into buffer
        newline = 0;
        while(!newline){
            received = recv(connection->client_fd, buffer + total_received, buf_size - total_received, 0);
            total_received += received;
            
            syslog(LOG_DEBUG, "total received: %zu", total_received);

            if (received == -1) {
                syslog(LOG_ERR, "receive error:%s\n", strerror(errno));
                free(buffer);
                close(connection->client_fd);
                connection->is_complete = 1;
                return NULL;
            }

            //check for newline
            for (int i = total_received - received; i<total_received; i++){
                if(buffer[i] == '\n'){
                    newline = 1;
                    break;
                }
            }

            //no newline yet, grow the buffer
            if (!newline && total_received == buf_size){
                buf_size *= 2;
                char *new_buf = realloc(buffer, buf_size);
                if (new_buf == NULL){ //makes sure realloc succeeds, and frees buffer if it doesn't to prevent memory leak
                    syslog(LOG_ERR, "Error growing buffer");
                    free(buffer);
                    close(connection->client_fd);
                    connection->is_complete = 1;
                    return NULL;
                }
                buffer = new_buf;
            }
            
            //syslog(LOG_DEBUG, "buffer %s", buffer);

            if(received == 0) //if the client closed the connection, stop receiving 
                break;

        }

        pthread_mutex_lock(&file_write_mutex);
        writefile_fd = open(WRITE_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
        write(writefile_fd, buffer, total_received);
        close(writefile_fd);
        readfile_fd = open(READ_FILE, O_RDONLY);

        do{
            read_count = read(readfile_fd, buffer, buf_size);
            
            if (read_count == -1) {
                syslog(LOG_ERR, "read file error:%s\n", strerror(errno));
                break;
            }

            if(read_count)
                send(connection->client_fd, buffer, read_count, 0);

        } while (read_count);

        close(readfile_fd);
        pthread_mutex_unlock(&file_write_mutex);
        free(buffer);
        close(connection->client_fd);
        syslog(LOG_INFO, "Closed connection from %s", ip_string);
        connection->is_complete = 1;

        return NULL;
}

int main(int argc, char *argv[]){
    //Start a logger
	openlog("Assignment6", 0, LOG_USER);

    //setup signal handler
    struct sigaction signal_action;
    memset(&signal_action, 0, sizeof(signal_action)); //make sure signal action is empty
    signal_action.sa_handler = signal_handler;
    sigaction(SIGINT, &signal_action, NULL);
    sigaction(SIGTERM, &signal_action, NULL);
    
    int status, sock_fd, their_fd;
    struct addrinfo hints;
    struct addrinfo *servinfo;
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    connection_node_t *node, *next_node;
    
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
    
    pthread_t timer_thread;
    pthread_create(&timer_thread, NULL, timer_handler, NULL);

    while(running){
        their_fd = accept(sock_fd, (struct sockaddr *)&their_addr, &addr_size);
        //addr_size is a pointer because it takes size of sockaddr_storage as an input and outputs actual size of the addr filled in
        //sockaddr_storage is a struct defined in socket.h, used to store incoming client's address of unknown type (and size) and 
            //guaranteed to be castable to sockaddr

        if (their_fd == -1) { //if there is an error in accept other that signal interrupt, exit program with error
            if(errno == EINTR) break; //if errno is EINTR, then a signal was received, exit the loop
            else{
                syslog(LOG_ERR, "accept error:%s\n", strerror(errno));
                exit(6);
            }
        }

        connection_node_t *new_conn = malloc(sizeof(connection_node_t));
        if (new_conn == NULL){
            syslog(LOG_ERR, "Unable to allocate memory for new connection node.");
            exit(11);
        }

        new_conn->client_fd = their_fd;
        new_conn->their_addr = their_addr;
        new_conn->addr_size = addr_size;
        new_conn->is_complete = 0;

        pthread_create(&new_conn->thread_id, NULL, connection_handler, new_conn);
        SLIST_INSERT_HEAD(&head, new_conn, entries);

        //finish handling new connection by cleaning up any completed old connections
        pthread_mutex_lock(&list_mutex);

        node = SLIST_FIRST(&head);
        while (node != NULL){
            next_node = SLIST_NEXT(node, entries);
            if (node->is_complete == 1){
                pthread_join(node->thread_id, NULL);
                SLIST_REMOVE(&head, node, connection_node, entries);
                free(node);
            }
            node = next_node;
        }
        
        pthread_mutex_unlock(&list_mutex);
        
    }
    
    node = SLIST_FIRST(&head);
    while (node != NULL){
        if(!node->is_complete){
            shutdown(node->client_fd, SHUT_RDWR);
        }
        next_node = SLIST_NEXT(node, entries);
        pthread_join(node->thread_id, NULL);
        SLIST_REMOVE(&head, node, connection_node, entries);
        free(node);
        node = next_node;
    }
    pthread_join(timer_thread, NULL);
    remove(WRITE_FILE);
    close(sock_fd);
    freeaddrinfo(servinfo);
    pthread_mutex_destroy(&list_mutex);
    pthread_mutex_destroy(&file_write_mutex);

    return 0;
}
