#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>

extern int errno;

int main (int argc, char *argv[]) 
{
    openlog("assignment-2-program", LOG_PID | LOG_CONS, LOG_USER);

    if (argc < 3)
    {
        /* insufficient arguments entered */
        printf("insufficient arguments entered \n\r");

        syslog(LOG_ERR, "Invalid arguments entered.");

        closelog();

        return 1;
    }

    int fd;
    ssize_t nr = 0;

    char *file_name = argv[1];
    char *str = argv[2];

    // printf("File name: %s and text: %s\n\r", file_name, str);
    syslog(LOG_DEBUG, "Writing %s to %s", str, file_name);

    fd = open(file_name, O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG);

    if (fd == -1)
    {
        /* error */
        syslog(LOG_ERR, "File creation failed (errno %d)", errno);
        printf("File creation failed (errno %d)\n", errno);
    }
    else
    {
        nr = write(fd, str, strlen(str));
            
        // printf("File wrote successfully.\n");

        if (close(fd) == -1)
        {
            /* error */
            syslog(LOG_ERR, "File close failed (errno %d)", errno);
            perror("close");
        }
    }

    if (nr == -1)
    {
        /* error */
        syslog(LOG_ERR, "Writing on file failed (errno %d)", errno);
        printf("Writing on file failed (errno %d)\n", errno);
    }

    closelog();

    return 0;
}