#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include "logging.h"
#define SOCAT "/usr/bin/socat"

char *socat_cmd[] = {SOCAT, "stdio", "/tmp/haproxy.socket", NULL};

#define SOCAT_INPUT "set weight https/s%d %d\r\n"

#define WRITE_END 1
#define READ_END 0

int reweight_haproxy(int server, int weight){

    int fd[2];
    pipe(fd);

    pid_t pid = fork();

    if (pid == 0) {
        dup2(fd[READ_END], STDIN_FILENO);
        close(fd[WRITE_END]);
        execv(SOCAT, socat_cmd);
        close(fd[READ_END]);
        log_error("Failed to execute '%s'\n", SOCAT);
        exit(1);
    } else {
       int status;
       FILE *f = fdopen(fd[WRITE_END], "w");
       if (f == NULL) {
           log_error("Error opening FILE to pipe");
       }
       int rtn = fprintf(f, SOCAT_INPUT, server, weight);
       if (rtn == -1) {
           perror("fprintf");
       }
       fclose(f);
       close(fd[READ_END]);
       log_info("written");
       waitpid(pid, &status, 0);
    }
    return 0;
}
