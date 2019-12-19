/*grupo 26*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "commandlinereader.h"

#define BUFFER_SIZE    100
#define MAXARGS        7
#define EXIT_COMMAND   "exit"
#define STATS_COMMAND  "stats"
#define PID_COMMAND    "pid"
#define UNPID_COMMAND  "unpid"
#define EXIT_GLOBAL_COMMAND   "exit-global"

int fd_shell;
static char buf_temp[BUFFER_SIZE] = "/tmp/pipe_XXXXXX";

void send_pid_command(char *pid_command){
    char command[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    char buf_pipe[BUFFER_SIZE];
    int fd_temp;

    strcpy(command, pid_command);
    strcat(command, " ");
    snprintf(buffer, 10, "%d", getpid());
    strcat(command, buffer);

    if(strncmp(command, PID_COMMAND, strlen(PID_COMMAND)) == 0){
        strcpy(buf_pipe, buf_temp);
        mktemp(buf_pipe);

        strcat(command, " ");
        strcat(command, buf_pipe);
        strcat(command, "\n");

        /*Make temporary pipe*/
        unlink(buf_pipe);
        if(mkfifo(buf_pipe, 0777) < 0){
            send_pid_command(UNPID_COMMAND);
            perror("Error making pipe");
            exit(EXIT_FAILURE);
        }  

        if(write(fd_shell, command, strlen(command)) < 0){
            send_pid_command(UNPID_COMMAND);
            perror("Error writing pipe");
            exit(EXIT_FAILURE);
        }

        /*Open temporary pipe*/
        if((fd_temp = open(buf_pipe, O_RDONLY)) < 0){
            send_pid_command(UNPID_COMMAND);
            perror("Error opening pipe");
            exit(EXIT_FAILURE);
        }

        strcpy(buffer, "\n");
        read(fd_temp, buffer, BUFFER_SIZE);
        printf("%s", buffer);
        sleep(1);
        /*Close temporary pipe*/
        if(close(fd_temp) < 0 ){
            perror("Error closing pipe");
            exit(EXIT_FAILURE);
        }
        unlink(buf_pipe);
        return;
    }

    strcat(command, "\n");
    if (write(fd_shell, command, strlen(command)) < 0){
        perror("Error writing pipe");
        exit(EXIT_FAILURE);
    }
}

void signal_handler_func(int signum){
    send_pid_command(UNPID_COMMAND);
    if(close(fd_shell) < 0 ){
        perror("Error closing pipe");
        exit(EXIT_FAILURE);
    }
    printf("***Exit\n");
    exit(0);
}



int main(int argc, char **argv){
	int fd_temp;
    char command[BUFFER_SIZE];
    char buf_pipe[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];

    signal(SIGINT, signal_handler_func);

    if (argc != 2) {
        printf("Invalid argument count.\n");
        printf("Usage:\n");
        printf("\t%s [MAIN_PIPE] \n\n", argv[0]);
        exit(EXIT_FAILURE);
    }

	if((fd_shell = open(argv[1], O_WRONLY)) < 0){
        perror("Error opening pipe");
        exit(EXIT_FAILURE);
    }

    send_pid_command(PID_COMMAND);


    /*Starts cicle*/
    while(fgets(command, BUFFER_SIZE, stdin) != NULL){

        if( ( (strncmp(command, PID_COMMAND, strlen(PID_COMMAND)) == 0) || 
            (strncmp(command, UNPID_COMMAND, strlen(UNPID_COMMAND)) == 0) ) &&
            ((command[strlen(PID_COMMAND)] == ' ') ||
            (command[strlen(UNPID_COMMAND)] == ' ' )) ){

            printf("Invalid command, unfortunately we can't run a program named \"pid\" or a program named \"unpid\"\n");
            continue;
        }

        if( (strncmp(command, EXIT_GLOBAL_COMMAND, strlen(EXIT_GLOBAL_COMMAND)) == 0) &&
            ( (command[strlen(EXIT_GLOBAL_COMMAND)] == ' ') || (command[strlen(EXIT_GLOBAL_COMMAND)] == '\n')) ) {

            strcpy(buffer, EXIT_GLOBAL_COMMAND);
            strcat(buffer, "\n");
            if (write(fd_shell, buffer, strlen(buffer)) < 0){
                send_pid_command(UNPID_COMMAND);
                perror("Error writing pipe");
                exit(EXIT_FAILURE);
            }
            printf("***Waiting to be killed\n");
            while(1){
                sleep(10);
            }
        }

        if( (strncmp(command, EXIT_COMMAND, strlen(EXIT_COMMAND)) == 0) &&
            ( (command[strlen(EXIT_COMMAND)] == ' ') || (command[strlen(EXIT_COMMAND)] == '\n')) ){

            break;
        }

        if( (strncmp(command, STATS_COMMAND, strlen(STATS_COMMAND)) == 0) &&
            ( (command[strlen(STATS_COMMAND)] == ' ') || (command[strlen(STATS_COMMAND)] == '\n')) ){

            strcpy(buf_pipe, buf_temp);
		    mktemp(buf_pipe);

		    strcpy(buffer, "stats ");
		    strcat(buffer, buf_pipe);
            strcat(buffer, "\n");

            /*Make temporary pipe*/
            unlink(buf_pipe);
            if(mkfifo(buf_pipe, 0777) < 0){
                send_pid_command(UNPID_COMMAND);
                perror("Error making pipe");
                exit(EXIT_FAILURE);
            }  
      
		    if(write(fd_shell, buffer, strlen(buffer)) < 0){
                send_pid_command(UNPID_COMMAND);
                perror("Error writing pipe");
                exit(EXIT_FAILURE);
            }

           
		    /*Open temporary pipe*/
		    if((fd_temp = open(buf_pipe, O_RDONLY)) < 0){
                send_pid_command(UNPID_COMMAND);
                perror("Error opening pipe");
                exit(EXIT_FAILURE);
            }

            read(fd_temp, buffer, BUFFER_SIZE);
            printf("%s\n", buffer);

            /*Close temporary pipe*/
            if(close(fd_temp) < 0 ){
                perror("Error closing pipe");
                exit(EXIT_FAILURE);
            }
            unlink(buf_pipe);
            continue;
        }

        if (write(fd_shell, command, strlen(command)) < 0){
            send_pid_command(UNPID_COMMAND);
            perror("Error writing pipe");
            exit(EXIT_FAILURE);
        }
    }

    send_pid_command(UNPID_COMMAND);

    /*Close pipe*/
    if(close(fd_shell) < 0 ){
        perror("Error closing pipe");
        exit(EXIT_FAILURE);
    }
	return 0;
}