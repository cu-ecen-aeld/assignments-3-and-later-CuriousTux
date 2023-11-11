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

#define     PORTNUM     "9000"
#define     BACKLOG     ((int)5)
#define     AESD_SOCKET_LOG_FILE    "/var/tmp/aesdsocketdata"
#define     MAX_BUFF_SIZE ((int) 22768)  

/* Global variables for singal handling */
volatile bool caught_sigint = false;
volatile bool caught_sigterm = false;

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
    // int recv_result;
    char c_ip[INET6_ADDRSTRLEN] = {0};
    char* recv_char = NULL;
    recv_char = malloc(22768);
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
        }

        /* Recieving and sending data on socket */
        ssize_t total_received = 0;
        bool end_of_packet = false;
        while (!end_of_packet && total_received < MAX_BUFF_SIZE - 1 && !(caught_sigint || caught_sigterm)) {
            ssize_t recv_result = recv(clientfd, recv_char + total_received, MAX_BUFF_SIZE - 1 - total_received, 0);
            if (recv_result == -1) {
                perror("recv");
                break;
            } else if (recv_result == 0) {
                end_of_packet = true;
                syslog(LOG_DEBUG, "Closed connection from %s", c_ip);
                printf("Client %s disconnected...\n", c_ip);
            } else {
                total_received += recv_result;
                // Check for newline or other delimiter indicating end of message
                if (strchr(recv_char, '\n') != NULL) {
                    end_of_packet = true;
                }
            }
        }

        recv_char[total_received] = '\0'; // Null-terminate the received string

        // Writing to the file (and any other processing)
        int fd = open(AESD_SOCKET_LOG_FILE, O_RDWR | O_CREAT | O_APPEND, S_IRWXU | S_IRWXG);
        if (fd != -1) {
            if (write_full(fd, recv_char, strlen(recv_char)) == -1) {
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
                        return 1;
                    }
                    strcpy(line, token);
                    strcat(line, "\n");
                    
                    // printf("Line: %s\n", line);
                    res = send(clientfd, line, strlen(line), MSG_CONFIRM);
                    if (res == -1)
                    {
                        perror("read");
                        printf("read error (%d)\n", errno);
                    }

                    token = strtok(NULL, "\n");
                }
            }

            // if (bytes_read == 0)
            // {
            //     printf("bytes_read = 0\n");
            // }

            if (bytes_read == -1) {
                perror("read");
            }

            close(fd);
            free(line);
        } else {
            perror("file open");
        }

        memset(recv_char, 0, MAX_BUFF_SIZE); // Reset buffer for next message
        total_received = 0;

        /*
        do
        {
            memset(recv_char, 0, 22768);
            recv_result = recv(clientfd, recv_char, 22768, 0); // MSG_TRUNC
            if (recv_result == -1)
            {
                perror("recv");
                break;
            }

            if (recv_result == 0)
            {
                end_of_packet = true;
                syslog(LOG_DEBUG, "Closed connection from %s",c_ip);
                printf("Client %s disconnected...\n", c_ip);
            }
            else
            {
                printf("recieved packet length: %d\n\r", recv_result);
                // printf("%s", recv_char);
                // if (strchr(recv_char, '\n') != NULL)
                // {
                    int fd = open(AESD_SOCKET_LOG_FILE, O_RDWR | O_CREAT | O_APPEND, S_IRWXU | S_IRWXG);
                    if (fd == -1)
                    {
                        perror("file open");
                        printf("File creation failed (errno %d)\n", errno);
                    }
                    else
                    {
                        // res = write(fd, recv_char, strlen(recv_char));
                        // printf("Wants to write: %lu\n\r", strlen(recv_char));
                        res = write_full(fd, recv_char, strlen(recv_char));
                        if (res == -1)
                        {
                            perror("file write");
                            printf("File write failed (errno %d)\n", errno);
                            close(fd);
                        }
                        else
                        {
                            // printf("Successfully wrote %s in file: %s\n\r", recv_char, AESD_SOCKET_LOG_FILE);
                            printf("Number of characters writter: %d\n", res);

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
                                        return 1;
                                    }
                                    strcpy(line, token);
                                    strcat(line, "\n");
                                    
                                    // printf("Line: %s\n", line);
                                    res = send(clientfd, line, strlen(line), MSG_CONFIRM);
                                    if (res == -1)
                                    {
                                        perror("read");
                                        printf("read error (%d)\n", errno);
                                    }

                                    token = strtok(NULL, "\n");
                                }
                            }

                            // if (bytes_read == 0)
                            // {
                            //     printf("bytes_read = 0\n");
                            // }

                            if (bytes_read == -1) {
                                perror("read");
                            }

                            close(fd);
                            free(line);
                        }
                    }
                // }
            }
            
        } while (!end_of_packet && (!(caught_sigint || caught_sigterm)));
        */
        
        shutdown(clientfd, SHUT_RDWR);

        // if (end_of_packet)
        // {
        //     break;
        // }
    }

    free(recv_char);

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