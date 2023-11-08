#include "systemcalls.h"
#include <stdlib.h>     // for system()
#include <unistd.h>     // for execl()
#include <sys/types.h>  // for pid_t datatype
#include <sys/wait.h>   // for waitpid()
#include <fcntl.h>      // for file functions
#include <errno.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

    int result = system(cmd);

    if (result == -1)
    {
        return false;
    }

    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    int status;
    pid_t pid;

    pid = fork();

    if (pid == -1)
    {
        va_end(args);
        return false;
    }
    else if (pid == 0)
    {
        execv(command[0], command);

        exit(-1);

        va_end(args);
        return false;
    }

    if (waitpid(pid, &status, 0) == -1)
    {
        va_end(args);
        return false;
    }
    else 
    {
        if (WIFEXITED(status)) 
        {
            int exit_status = WEXITSTATUS(status);
            va_end(args);
            return exit_status == 0;
        } 
        else 
        {
            va_end(args);
            return false;
        }
    }

    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;


    pid_t pid;

    int fd = open(outputfile, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (fd == -1)
    {
        perror("File open failed");
        va_end(args);
        return false;
    }

    pid = fork();

    if (pid == -1)
    {
        perror("Fork error");
        va_end(args);
        close(fd);
        return false;
    }
    else if (pid == 0)
    {
        if (dup2(fd, STDOUT_FILENO) < 0)
        {
            perror("dup2");
            va_end(args);
            return false;
        }

        close(fd);

        execv(command[0], command);

        perror("execv");
        va_end(args);
        return false;
    }
    else
    {
        close(fd);

        int status;

        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            va_end(args);
            return false;
        }

        if (WIFEXITED(status)) {
            printf("Child exited with status %d\n", WEXITSTATUS(status));
        }
    }

    va_end(args);

    return true;
}
