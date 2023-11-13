#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#include "queue.h"  // queue taken from FreeBSD 10
#include <time.h>

/* Macros and constants */
#define     PORTNUM     "9000"
#define     BACKLOG     ((int)5)
#define     AESD_SOCKET_LOG_FILE    "/var/tmp/aesdsocketdata"
#define     MAX_BUFF_SIZE ((int) 22768)  

/* Type definitions and structures */
struct thread_data
{
    pthread_mutex_t* mutex;
    bool isRunning;
    int clientfd;
    char clientIP[INET6_ADDRSTRLEN];
};

struct node
{
    pthread_t thread;
    struct thread_data t_data;
    TAILQ_ENTRY(node) nodes;
};


/* Global variables for singal handling */
volatile bool caught_sigint = false;
volatile bool caught_sigterm = false;

/* Functions */
static void * socketTransmitionTask(void * thread);
static void * timerTask(void * thread);
static void signalHandlers(int signum);
static ssize_t write_full(int fd, const char *buffer, size_t count);
static void getTimestamp(char *timestamp, size_t size);



int main(int argc, char *argv[])
{
    /* Enabling syslog in the program */
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);

    /* Enabling signal handling */
    struct sigaction new_action;
    
    memset(&new_action, 0, sizeof(struct sigaction));
    new_action.sa_handler = signalHandlers;

    if (sigaction(SIGTERM, &new_action, NULL) != 0)
    {
        perror("sigaction (SIGTERM)");
        closelog();
        return 1;
    }
    if (sigaction(SIGINT, &new_action, NULL) != 0)
    {
        perror("sigaction (SIGINT)");
        closelog();
        return 1;
    }

    /* Opening socket */
    int socketfd;
    struct addrinfo hints;
    struct addrinfo *info;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL, PORTNUM, &hints, &info);

    /* Make the Socket */
    printf("Trying to make the socket\n\r");
    socketfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (socketfd == -1)
    {
        perror("socket");
        closelog();
        return -1;
    }    

    /* Set SO_REUSEADDR */
    int optval = 1;
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(socketfd);
        return -1;
    }

    /* Bind the port */
    printf("Trying to bind the socket\n\r");
    int res = bind(socketfd, info->ai_addr, info->ai_addrlen);
    if (res == -1)
    {
        shutdown(socketfd, SHUT_RDWR);
        freeaddrinfo(info);
        perror("bind");
        printf("Bind error\n\r");
        closelog();
        return -1;
    }

    freeaddrinfo(info);

    /* Checking the deamon mode */
    int daemon_mode = 0;

    if (argc > 1)
    {
        if (strcmp(argv[1], "-d") == 0)
        {
            daemon_mode = 1;
        }
    }

    if (daemon_mode)
    {
        pid_t pid = fork();

        if (pid < 0)
        {
            exit(EXIT_FAILURE);
        }
        
        if (pid > 0)
        {
            exit(EXIT_SUCCESS);
        }

        if (setsid() < 0)
        {
            exit(EXIT_FAILURE);
        }

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        closelog();
    }

    /* Listen to the port */
    printf("Trying to listen to the socket\n\r");
    res = listen(socketfd, BACKLOG);
    if (res == -1)
    {
        shutdown(socketfd, SHUT_RDWR);
        perror("listen");
        closelog();
        return -1;
    }

    socklen_t client_addr_size;
    struct sockaddr client_addr;
    int clientfd;
    char c_ip[INET6_ADDRSTRLEN] = {0};

    TAILQ_HEAD(head_s, node) head;
    TAILQ_INIT(&head);
    struct node * nd = NULL;

    pthread_mutex_t f_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    struct thread_data timer_thread_data = {.isRunning = false, .mutex = &f_mutex};
    pthread_t timer_thread;
    
    if (pthread_create(&timer_thread, NULL, timerTask, (void *) &timer_thread_data) == 0)
    {
        timer_thread_data.isRunning = true;
        printf("Timer thread started...\n");
    }
    else
    {
        perror("timer thread");
    }
    
    int tCounter = 0;
    while (!(caught_sigint || caught_sigterm))
    {
        /* New client comming, accept the client */
        client_addr_size = sizeof client_addr;
        memset(c_ip, 0, INET6_ADDRSTRLEN);
        clientfd = accept(socketfd, (struct sockaddr *)&client_addr, &client_addr_size);
        if (clientfd == -1)
        {
            perror("accept");
            continue;
        }
        else
        {
            inet_ntop(AF_INET, &client_addr.sa_data, c_ip, INET_ADDRSTRLEN);
            syslog(LOG_DEBUG, "Accepted connection from %s", c_ip);
            printf("Accepted connection from %s\n", c_ip);
        }

        // TODO: start the thread here
        nd = malloc(sizeof(struct node));
        if (nd == NULL)
        {
            perror("node malloc");
            continue;
        }

        nd->t_data.clientfd = clientfd;
        memcpy(nd->t_data.clientIP, c_ip, INET6_ADDRSTRLEN);
        nd->t_data.isRunning = false;
        nd->t_data.mutex = &f_mutex;

        /* Recieving and sending data on socket */
        int t_status = pthread_create(&nd->thread, NULL, socketTransmitionTask, (void *) &nd->t_data);
        if (t_status == 0)
        {
            printf("Thread %d started\n", tCounter);
            // pthread_join(thread[tCounter], NULL);
            tCounter++;

            TAILQ_INSERT_TAIL(&head, nd, nodes);
        }
        else
        {
            perror("pthread_create");
        }

        nd = NULL;
        
    }

    // TODO: handle pthread_join here?
    TAILQ_FOREACH(nd, &head, nodes)
    {
        pthread_join(nd->thread, NULL);
    }

    pthread_join(timer_thread, NULL);

    pthread_mutex_destroy(&f_mutex);

    while(!TAILQ_EMPTY(&head))
    {
        nd = TAILQ_FIRST(&head);
        TAILQ_REMOVE(&head, nd, nodes);
        free(nd);
        nd = NULL;
    }

    if (caught_sigint || caught_sigterm)
    {
        syslog(LOG_DEBUG, "Caught signal, exiting");
        printf("Signal caught...\n\r");
    }

    /* Cleaning up the memory */
    if (remove(AESD_SOCKET_LOG_FILE) == 0)
    {
        printf("File removed successfully...\n\r");
    }
    else
    {
        printf("File remove failed\n");
    }
    shutdown(socketfd, SHUT_RDWR);
    closelog();

    return 0;
}

/* Signal handlers */
static void signalHandlers(int signum)
{
    // Handling the SIGINT and SIGTERM signals
    if (signum == SIGINT)
    {
        caught_sigint = true;
    }
    else if (signum == SIGTERM)
    {
        caught_sigterm = true;
    }
    remove(AESD_SOCKET_LOG_FILE);
}

/* File handlers and helpers */
static ssize_t write_full(int fd, const char *buffer, size_t count) {
    size_t bytes_written = 0;
    ssize_t result;

    while (bytes_written < count) {
        result = write(fd, buffer + bytes_written, count - bytes_written);

        if (result < 0) {
            if (errno == EINTR) {
                // The write call was interrupted; try again
                continue;
            } else {
                // An actual error occurred
                return -1;
            }
        }

        bytes_written += result;
    }

    return bytes_written;
}

/* Time helper function to get the timestamp in RFC 2822 format */
static void getTimestamp(char *timestamp, size_t size) {
    time_t rawtime;
    struct tm *info;

    time(&rawtime);
    info = localtime(&rawtime);

    strftime(timestamp, size, "timestamp:%Y-%m-%d %H:%M:%S", info);
}

/* Thread handlers and tasks */
static void * socketTransmitionTask(void * t_data)
{
    ssize_t total_received = 0;
    bool end_of_packet = false;
    bool disconnected = false;
    char* recv_buffer = NULL;
    recv_buffer = malloc(MAX_BUFF_SIZE);

    struct thread_data* thread = (struct thread_data*) t_data;

    while (!disconnected && total_received < MAX_BUFF_SIZE - 1 && !(caught_sigint || caught_sigterm)) 
    {
        ssize_t recv_result = recv(thread->clientfd, recv_buffer + total_received, MAX_BUFF_SIZE - 1 - total_received, 0);
        if (recv_result == -1) {
            perror("recv");
            break;
        } else if (recv_result == 0) {
            disconnected = true;
            syslog(LOG_DEBUG, "Closed connection from %s", thread->clientIP);
            printf("Client %s disconnected...\n", thread->clientIP);
        } else {
            total_received += recv_result;
            // Check for newline or other delimiter indicating end of message
            if (strchr(recv_buffer, '\n') != NULL) {
                end_of_packet = true;
            }
        }

        if (end_of_packet)
        {
            // Lock mutex
            int mres = pthread_mutex_lock(thread->mutex);
            if (mres == 0)
            {
                recv_buffer[total_received] = '\0'; // Null-terminate the received string

                // Writing to the file (and any other processing)
                int fd = open(AESD_SOCKET_LOG_FILE, O_RDWR | O_CREAT | O_APPEND, S_IRWXU | S_IRWXG);
                if (fd != -1) 
                {
                    if (write_full(fd, recv_buffer, strlen(recv_buffer)) == -1) 
                    {
                        perror("file write");
                    }

                    if (lseek(fd, 0, SEEK_SET) == -1)
                    {
                        perror("lseek");
                        close(fd);
                        continue;
                    }

                    char buffer[MAX_BUFF_SIZE];
                    int bytes_read;
                    char *line = NULL;
                    size_t len = 0;

                    while ((bytes_read = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
                        buffer[bytes_read] = '\0';
                        char *token = strtok(buffer, "\n");

                        while (token != NULL) {
                            len = strlen(token);
                            line = realloc(line, len + 2);
                            if (line == NULL) {
                                perror("realloc");
                                close(fd);
                                free(line);
                                free(recv_buffer);
                                shutdown(thread->clientfd, SHUT_RDWR);
                                return t_data;
                            }
                            strcpy(line, token);
                            strcat(line, "\n");
                            
                            // printf("Line: %s\n", line);
                            int res = send(thread->clientfd, line, strlen(line), MSG_CONFIRM);
                            if (res == -1)
                            {
                                perror("read");
                                printf("read error (%d)\n", errno);
                            }

                            token = strtok(NULL, "\n");
                        }
                    }

                    if (bytes_read == -1) {
                        perror("read");
                    }

                    close(fd);
                    free(line);
                } 
                else 
                {
                    perror("file open");
                }

                mres = pthread_mutex_unlock(thread->mutex);
            }



            end_of_packet = false;
            memset(recv_buffer, 0, MAX_BUFF_SIZE); // Reset buffer for next message
            total_received = 0;
        }

    }

    free(recv_buffer);
    shutdown(thread->clientfd, SHUT_RDWR);
    return t_data;
}


static void * timerTask(void * t_data)
{
    int mres = 0;
    struct thread_data* thread = (struct thread_data*) t_data;

    while (!(caught_sigint || caught_sigterm)) {

        /* Append timestamp to the file every 10 seconds */
        // Lock mutex
        mres = pthread_mutex_lock(thread->mutex);
        if (mres == 0)
        {
            char timestamp[64];  // Adjust the size accordingly
            getTimestamp(timestamp, sizeof(timestamp));
            strcat(timestamp, "\n");

            int fd = open(AESD_SOCKET_LOG_FILE, O_RDWR | O_CREAT | O_APPEND, S_IRWXU | S_IRWXG);
            if (fd != -1) {
                if (write_full(fd, timestamp, strlen(timestamp)) == -1) {
                    perror("file write");
                }
                close(fd);
            } else {
                perror("file open");
            }

            // Unlock mutex
            mres = pthread_mutex_unlock(thread->mutex);
        }

        sleep(10);

    }

    thread->isRunning = false;

    return thread;
}


