/*grupo 26*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#include "commandlinereader.h"
#include "list.h"

#define EXIT_COMMAND   "exit"
#define STATS_COMMAND  "stats"
#define PID_COMMAND	   "pid"
#define UNPID_COMMAND  "unpid"
#define EXIT_GLOBAL_COMMAND   "exit-global"
#define MAXARGS        7
#define BUFFER_SIZE    100
#define MAXPAR         5
#define MAXCLIENTS     5
#define LOG_FILE       "log.txt"
#define MAIN_PIPE	   "/tmp/par-shell-in"

/*****************************************************
 * Global variables. *********************************
 *****************************************************/

int i;
int total = 0;
int max_concurrency;
int max_clients;
int num_children = 0;
int flag_exit = 0;
pthread_t monitor_tid;

list_t *proc_data;
pthread_mutex_t data_ctrl;
pthread_cond_t max_concurrency_ctrl;
pthread_cond_t no_command_ctrl;


int lst_pid[MAXCLIENTS];
int n_pid = 0;




/*****************************************************
 * Helper functions. *********************************
 *****************************************************/

void m_lock(pthread_mutex_t* mutex) {
  	if (pthread_mutex_lock(mutex)) {
    	perror("Error locking mutex");
    	exit(EXIT_FAILURE);
  	}
}

void m_unlock(pthread_mutex_t* mutex) {
  	if (pthread_mutex_unlock(mutex)) {
    	perror("Error unlocking mutex");
    	exit(EXIT_FAILURE);
  	}
}

void c_wait(pthread_cond_t* condition, pthread_mutex_t* mutex) {
  	if (pthread_cond_wait(condition, mutex)) {
    	perror("Error waiting on condition");
    	exit(EXIT_FAILURE);
 	}
}

void c_signal(pthread_cond_t* condition) {
  	if (pthread_cond_signal(condition)) {
    	perror("Error signaling on condition");
    	exit(EXIT_FAILURE);
  	}
}

void exit_function(){
    m_lock(&data_ctrl);
    flag_exit = 1;
    c_signal(&no_command_ctrl);
    m_unlock(&data_ctrl);

    /*synchronize with additional threads*/
    if (pthread_join(monitor_tid, NULL)) {
      perror("Error joining thread 1");
      exit(EXIT_FAILURE);
    }

    lst_print(proc_data);

    /*clean up*/
    pthread_mutex_destroy(&data_ctrl);
    if (max_concurrency > 0 && pthread_cond_destroy(&max_concurrency_ctrl)) {
        perror("Error destroying condition");
        exit(EXIT_FAILURE);
    }
    if(pthread_cond_destroy(&no_command_ctrl)) {
        perror("Error destroying condition");
        exit(EXIT_FAILURE);
    }

    lst_destroy(proc_data);
    unlink(MAIN_PIPE);

}

void par_shell_out_pid(int pid){
  	int fd;
  	char str1[] = "par-shell-out-", str2[10];
  	snprintf(str2, 10, "%d", pid);
  	strcat(str1, str2);
  	strcat(str1, ".txt");
  	fd = open(str1, O_CREAT | O_WRONLY , S_IRUSR | S_IWUSR);
    if(close(1) < 0 ){
        perror("Error closing stdout");
        exit(EXIT_FAILURE);
    }
  	if(dup(fd) < 0){
        perror("Error duplicating pipe");
        exit(EXIT_FAILURE);
    }
}

void stats_info(char *str){
  	static char str1[]="Number of children in execution: ";
  	char str_aux[BUFFER_SIZE];
  	strcpy(str,str1);

  	m_lock(&data_ctrl);
  	snprintf(str_aux, 10, "%d", num_children);
  	strcat(str, str_aux);
  	strcat(str, "\tTotal execution time: ");
  	snprintf(str_aux, 10, "%d", total);
  	m_unlock(&data_ctrl);

  	strcat(str, str_aux);
  	strcat(str, " s\n");
}

void insert_pid(int pid, int fd){
    char buffer[BUFFER_SIZE];

    strcpy(buffer, "***Server is full\n");
    /*Full server*/
    if (n_pid >= max_clients) {
        if (write(fd, buffer, strlen(buffer)) < 0){
            perror("Error writing pipe");
            exit(EXIT_FAILURE);
        }
        sleep(1);
        if(kill(pid, SIGKILL) < 0){
            perror("Error killing terminal\n");
            exit(EXIT_FAILURE);
        }
        return;
    }

    /*Server not full*/
    lst_pid[n_pid] = pid;
    n_pid++;

    if(n_pid == max_clients){
        printf("***Server is full\n");
    }
}

int eliminate_pid(int pid){
	int a = 0, b = 0;

    while(a <= n_pid){
        if(lst_pid[a] == pid){
            n_pid--;
            lst_pid[a] = lst_pid[n_pid];
            b = 0;
            break;
        }
        a++;
        b = -1;
    }

    if((n_pid+1) == max_clients){
        printf("***Server not full\n");
    }
    return b;
}
	
	


void kill_clients(){
	n_pid--;
	while(n_pid >= 0){
        if(kill(lst_pid[n_pid], SIGKILL) < 0){
            perror("Error killing terminal\n");
            exit(EXIT_FAILURE);
        }
        printf("Terminal with pid %d killed\n", lst_pid[n_pid]);
        n_pid--;
    }
}

void signal_handler_func(int signum){
	kill_clients();
    exit_function();

    exit(0);
}



/*****************************************************
 * Monitor task function. ****************************
 *****************************************************/
void *monitor(void *arg_ptr) {
  	FILE *log_file;
  	int status, pid;
  	time_t end_time;

  	log_file = fopen(LOG_FILE, "a");
  	if (log_file == NULL) {
    	perror("Error opening file");
    	exit(EXIT_FAILURE);
  	}

  	while (1) {
    	/*wait for effective command condition*/
    	m_lock(&data_ctrl);
    	while (num_children == 0 && flag_exit == 0) {
      		c_wait(&no_command_ctrl, &data_ctrl);
    	}
    	if (flag_exit == 1 && num_children == 0) {
      		m_unlock(&data_ctrl);
      		break;
    	}
    	m_unlock(&data_ctrl);

    	/*wait for child*/
    	pid = wait(&status);
    	if (pid == -1) {
      		perror("Error waiting for child");
      		exit(EXIT_FAILURE);
    	}

    	/*register child performance and signal concurrency condition*/
   	 	end_time = time(NULL);
    	m_lock(&data_ctrl);
    	--num_children;
    	update_terminated_process(proc_data, pid, end_time, status);
    	int duration = end_time - process_start(proc_data, pid);
    	if (max_concurrency > 0) {
      		c_signal(&max_concurrency_ctrl);
    	}
    	m_unlock(&data_ctrl);

    	/*print execution time to disk*/
    	++i;
    	total += duration;
    	fprintf(log_file, "iteracao %d\npid: %d execution time: %d s\ntotal execution time: %d s\n", i, pid, duration, total);
    	if (fflush(log_file)) {
      		perror("Error flushing file");
      		exit(EXIT_FAILURE);
    	}
  	}

  	if (fclose(log_file)) {
    	perror("Error closing file");
    	exit(EXIT_FAILURE);
  	}

  	pthread_exit(NULL);
}

/*****************************************************
 * Main thread. **************************************
 *****************************************************/
int main(int argc, char **argv) {
  	int numargs = 0;
  	char buffer[BUFFER_SIZE];
  	char buf_stats_info[BUFFER_SIZE];
  	char *args[MAXARGS];
  	time_t start_time;
  	int pid, duration;
  	FILE *log_file;
  	int fd_temp, fd_shell;

  	signal(SIGINT, signal_handler_func);

  	if (argc != 1 && argc != 2 && argc != 3) {
    	printf("Invalid argument count.\n");
    	printf("Usage:\n");
    	printf("\t%s [MAXPAR]\n\n", argv[0]);
        printf("or\n");
        printf("\t%s [MAXPAR] [MAXCLIENTS] \n\n", argv[0]);
    	exit(EXIT_FAILURE);
  	}

  	max_concurrency = MAXPAR;
  	max_clients = MAXCLIENTS;

  	if (argc == 2) {
    	max_concurrency = atoi(argv[1]);
    	if (max_concurrency <= 0) {
     	 	printf("Invalid maximum concurrency - must be positive integer.\n");
      		exit(EXIT_FAILURE);
    	}
  	}
    else if(argc == 3){
        max_clients = atoi(argv[2]);
        if (max_clients <= 0) {
            printf("Invalid maximum clients - must be positive integer.\n");
            exit(EXIT_FAILURE);
        }
    }

  	/*initialize condition variables*/
  	if (max_concurrency > 0 && pthread_cond_init(&max_concurrency_ctrl, NULL)) {
    	perror("Error initializing condition");
    	exit(EXIT_FAILURE);
  	}

  	if (pthread_cond_init(&no_command_ctrl, NULL)) {
    	perror("Error initializing condition");
    	exit(EXIT_FAILURE);
  	}

  	proc_data = lst_new();

  	/*initialize proc_data*/
  	log_file = fopen(LOG_FILE, "r");
  	if (log_file == NULL) {
    	i = -1; /*will be incremented later*/
  	}

  	if (log_file) {
    	while (fgets(buffer, BUFFER_SIZE, log_file)) {
      		if (sscanf(buffer, "iteracao %d", &i)) {
        		continue;
      		}
      		if (sscanf(buffer, "total execution time: %d", &duration)) {
        		continue;
      		}
      		if (sscanf(buffer, "pid: %d execution time: %d s", &pid, &duration)) {
        		total += duration;
      		}
    	}

    	if (fclose(log_file)) {
    		perror("Error closing file");
      		exit(EXIT_FAILURE);
    	}
  	}

  	/*initialize mutex*/
  	if (pthread_mutex_init(&data_ctrl, NULL)) {
    	perror("Error initializing mutex");
   	 	exit(EXIT_FAILURE);
  	}

    /*create additional threads*/
    if (pthread_create(&monitor_tid, NULL, monitor, NULL) != 0) {
        perror("Error creating thread");
        exit(EXIT_FAILURE);
    }

  	printf("Child processes concurrency limit: %d", max_concurrency);
  	(max_concurrency == 0) ? printf(" (sem limite)\n\n") : printf("\n\n");
  
  	/*Make and open pipe*/
  	unlink(MAIN_PIPE);
  	if(mkfifo(MAIN_PIPE, 0777) < 0){
    	perror("Error making pipe");
    	exit(EXIT_FAILURE);
  	} 
  	if((fd_shell = open(MAIN_PIPE, O_RDONLY)) < 0){
    	perror("Error opening pipe");
   	 	exit(EXIT_FAILURE);
  	}
    if(close(0) < 0 ){
        perror("stdin");
        exit(EXIT_FAILURE);
    }
    if(dup(fd_shell) < 0){
        perror("Error duplicating pipe");
        exit(EXIT_FAILURE);
    }

  	printf("***Connected to first client by %s\n", MAIN_PIPE);


  	while(1){

	    numargs = readLineArguments(args, MAXARGS, buffer, BUFFER_SIZE); 
        
	    if (numargs < 0) {
	    	printf("Waiting for new client.\n");
	      	if((fd_shell = open(MAIN_PIPE, O_RDONLY)) < 0){
	    		perror("Error re-opening pipe\n");
	    		exit(EXIT_FAILURE);
	  	  	}
	  	  	continue; 
	    }
	    if (numargs == 0) {
	        continue;
	    }
	    
	    if(strncmp(args[0], EXIT_GLOBAL_COMMAND, strlen(EXIT_GLOBAL_COMMAND)) == 0){
	      	m_lock(&data_ctrl);	      	
	        kill_clients();
	        m_unlock(&data_ctrl);
	    	break;
	    }

	    if(strcmp(args[0], EXIT_COMMAND) == 0) {
	      break;
	    }

	    if(strcmp(args[0], PID_COMMAND) == 0){
            if((fd_temp = open(args[2], O_WRONLY)) < 0){
                perror("Error opening pipe");
                exit(EXIT_FAILURE);
            }

	    	m_lock(&data_ctrl);
	    	insert_pid(atoi(args[1]), fd_temp);				
	    	m_unlock(&data_ctrl);

            if(close(fd_temp) < 0 ){
                perror("Error closing pipe");
                exit(EXIT_FAILURE);
            }
	    	continue;
	    }

	    if(strcmp(args[0], UNPID_COMMAND) == 0){
	    	m_lock(&data_ctrl);
	    	if (eliminate_pid(atoi(args[1])) == -1){
				perror("Error eliminating client\n");
	    		exit(EXIT_FAILURE);
	    	}
	    	m_unlock(&data_ctrl);
	    	continue;
	    }

	    if (strcmp(args[0], STATS_COMMAND) == 0){
		    if((fd_temp = open(args[1], O_WRONLY)) < 0){
		        perror("Error opening pipe");
		        exit(EXIT_FAILURE);
		    }

	        stats_info(buf_stats_info);
	        if (write(fd_temp, buf_stats_info, strlen(buf_stats_info)) < 0){
	      	    perror("Error writing pipe");
	            exit(EXIT_FAILURE);
	        }
            if(close(fd_temp) < 0 ){
                perror("Error closing pipe");
                exit(EXIT_FAILURE);
            }
	        continue;
	    }

	    if (max_concurrency > 0) {
	      	m_lock(&data_ctrl);
	      	while (num_children == max_concurrency) {
	        	c_wait(&max_concurrency_ctrl, &data_ctrl);
	     	}
	      	m_unlock(&data_ctrl);
	    }

	    start_time = time(NULL);

	    /*create child process*/
	    pid = fork();
	    if (pid == -1) {
	      	c_signal(&max_concurrency_ctrl);
	      	continue;
	    }
	    if (pid > 0) {
	      	m_lock(&data_ctrl);
	      	++num_children;
	      	insert_new_process(proc_data, pid, start_time);
	      	c_signal(&no_command_ctrl);
	      	m_unlock(&data_ctrl);
	    }
	    if (pid == 0) {
	      	par_shell_out_pid(getpid());
            signal(SIGINT, SIG_IGN);
	      	if (execv(args[0], args) == -1) {
	        	perror("Error executing command");
	        	exit(EXIT_FAILURE);
	      	}
	    }
  	}

  	exit_function();
    if(close(fd_shell) < 0 ){
        perror("Error closing pipe");
        exit(EXIT_FAILURE);
    }

  	return 0;
}
