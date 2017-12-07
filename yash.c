/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.c
 * Author: leon
 *
 * Created on September 16, 2017, 10:11 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <termios.h>
#include <fcntl.h>



#define TRUE 1
#define FALSE 0

#define EXECUTABLE 0
#define ARGUMENT 1
#define IN 2
#define OUT 3
#define ERR 4

#define RUNNING 0
#define STOPPED 1
#define DONE 2

#define ALLJOBS 0
#define DONEJOBS 1



/*----command line parsing----*/
struct process {
    char *params[32];/*arguments*/
    char *inRedir;
    char *outRedir;
    char *errRedir;
    pid_t pid;
};

struct job {
    int job_num;
    char str[256];/*the original string of the command*/
    struct job *next;/*the next job*/
    pid_t child1_pid;/*pointer to first process in this job*/
    pid_t child2_pid;
    int status;/*RUNNING, STOPPED, DONE*/
    int bg;
};
struct job * firstJob = NULL;

void addProcToJob(struct job * job, struct process process){
    if(job->child1_pid == 0){
        job->child1_pid = process.pid;
    }
    else{
        job->child2_pid = process.pid;
    }
}

void addJobToJobList(struct job *job){
    if(firstJob == NULL){
        firstJob = job;
        firstJob->job_num = 1;
    }
    
    else{
        struct job * curJob = firstJob;
        while(curJob->next != NULL){
            curJob = curJob->next;
        }
        job->job_num = curJob->job_num + 1;
        curJob->next = job;
    }
}

int find_last_undone(){
    int pid;
    struct job *cur = firstJob;
    while(cur != NULL){
        if(cur->status != DONE){
            pid = cur->child1_pid;
        }
        cur = cur->next;
    }
    return pid;
}

void makefg(int target_pid){
    struct job *cur = firstJob;
    while(cur != NULL){
        if(cur->child1_pid == target_pid){
            cur->bg = FALSE;
            cur->status = RUNNING;
            printf("%s\n", cur->str);
            return;
        }
        cur = cur->next;
    }
}

void makebg(int target_pid){
    struct job *cur = firstJob;
    while(cur != NULL){
        if(cur->child1_pid == target_pid){
            cur->bg = TRUE;
            cur->status = RUNNING;
            printf("%s\n", cur->str);
            return;
        }
        cur = cur->next;
    }
}

void printJobs(int all_or_done){
    if(firstJob == NULL) return;
    
    int last_undone_pid = find_last_undone();
    
    char toPrint[256];
    struct job *cur = firstJob;
    while(cur != NULL){
        if((all_or_done) == DONEJOBS){
            if(cur->status != DONE){
                cur = cur->next;
                continue;
            }
        }
        strcat(toPrint, "[");

        /*append job num*/
        char buff[5];
        snprintf(buff, 5, "%d", cur->job_num);
        strcat(toPrint, buff);
        
        strcat(toPrint, "]");
        
        /* + or -*/
        if(cur->child1_pid == last_undone_pid){
            strcat(toPrint, "+  ");
        }
        else{
            strcat(toPrint, "-  ");
        }
        
        switch(cur->status){
            case RUNNING:
            {
                strcat(toPrint, "Running                 ");
                break;
            }
            case STOPPED:
            {
                strcat(toPrint, "Stopped                 ");
                break;
            }
            case DONE:
            {
                strcat(toPrint, "Done                    ");
                break;
            }
        }

        strcat(toPrint, cur->str);
        printf("%s\n", toPrint);
        strcpy(toPrint, "");
        cur = cur->next;
    }
}



void promptForInput(){
    printf("# ");
}
void getLine(char *line){
    char l[1000] = "";
    int counter = 0;
    while(TRUE){
        char curChar = getchar();    
        if(curChar == '\n'){
            break;
        }
        else{
            l[counter] = curChar;
            counter ++;
        }
    }
    strcpy(line, l);
}
void parseProcesses (char* line, char **proc1_str, char **proc2_str){
    char *token = "";
    token = strtok(line, "|");
    while (token != NULL){
        if (*proc1_str == NULL){
            *proc1_str = strdup(token);
        }
        else{
            *proc2_str = strdup(token);
        }
        token = strtok(NULL, "|");
    }
}
struct process parseSingleProcess(char* line){
    struct process proc;
    proc.errRedir = NULL;
    proc.inRedir = NULL;
    proc.outRedir = NULL;
    /*parse line*/
    char *token;
    int counter = 0;
    token = strtok(line, " \n");
    int tokenType = EXECUTABLE;/*initially we are looking for the name of the executable*/

    while(token != NULL && strcmp(token, "") != 0){
        switch(tokenType){
            case EXECUTABLE:
            {
                proc.params[counter] = strdup(token);
                tokenType = ARGUMENT;
                counter ++;
                break;
            }
            case ARGUMENT:
            {
                proc.params[counter] = strdup(token);
                counter ++;
                break;
            }
            case IN:
            {
                proc.inRedir = strdup(token);
                break;
            }
            case OUT:
            {
                proc.outRedir = strdup(token);
                break;
            }
            case ERR:
            {
                proc.errRedir = strdup(token);
                break;
            }

        }
        
        token = strtok(NULL, " \n");
        if(token == NULL) break;
        
        /*see if there are file redirections*/
        if(strcmp(token, ">") == 0){
            tokenType = OUT;
            token = strtok(NULL, " \n");
        }
        else if(strcmp(token, "<") == 0){
            tokenType = IN;
            token = strtok(NULL, " \n");
        }
        else if(strcmp(token, "2>") == 0){
            tokenType = ERR;
            token = strtok(NULL, " \n");
        }
    }
    proc.params[counter] = NULL;
    return proc;
}
int checkBg(struct process *proc){
    if(&proc == NULL){
        return FALSE;
    }
    int i = 0;
    while(proc->params[i] != NULL) i++;
    
    if(i == 0) return FALSE;
    else if(strcmp(proc->params[i-1],"&") == 0){
        proc->params[i-1] = NULL;
        return TRUE;
    }
    else return FALSE;
}


struct job *find_fg_job(){
    struct job * cur = firstJob;
    while(cur != NULL){
        if(cur->bg == FALSE){
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

int delete_job(int pid_to_delete){
    if(firstJob == NULL) return 0;
    if(firstJob->child1_pid == pid_to_delete){
        struct job* temp = firstJob;
        firstJob = firstJob->next;
        free(temp);
        return 1;
    }

    struct job* prev = NULL;
    struct job* cur = firstJob;
    
    while(cur != NULL){
        if(cur->child1_pid == pid_to_delete){
            prev->next = cur->next;
            free(cur);
            return 1;
        }
        prev = cur;
        cur = cur->next;
    }
    return 0;
}



int stop_job(int pid_to_stop){
    struct job* cur = firstJob;
    while(cur != NULL){
        if(cur->child1_pid == pid_to_stop){
            cur->status = STOPPED;
            cur->bg = TRUE;
            return 1;
        }
        cur = cur->next;
    }
    return 0;
}

int make_job_status_done(int pid_to_mark_done){
    struct job* cur = firstJob;
    while(cur != NULL){
        if(cur->child1_pid == pid_to_mark_done){
            cur->status = DONE;
            cur->bg = TRUE;
            return 1;
        }
        cur = cur->next;
    }
    return 0;
}

int delete_first_zombie(){
    if(firstJob == NULL){
        return 0;
    }
    
    if(firstJob->status == DONE){
        struct job* temp = firstJob;
        firstJob = firstJob->next;
        free(temp);
        return 1;

    }

    struct job* prev = NULL;
    struct job* cur = firstJob;
    
    while(cur != NULL){
        if(firstJob->status == DONE){
            prev->next = cur->next;
            free(cur);
            return 1;
        }
        prev = cur;
        cur = cur->next;
    }
    return 0;
}


int pipefd[2];

int child1_pid, child2_pid, pid;

int status;

void sigint_handler(int signo) {
    struct job *job = find_fg_job(firstJob);
    if (job != NULL) {
        printf("\n");
        kill(-job->child1_pid, SIGINT);
        fflush(stdout);
        delete_job(job->child1_pid);
    }
    else{
        printf("\n# ");
        fflush(stdout);
    }
}

void sigtstp_handler(int signo){
    struct job *job = find_fg_job(firstJob);
        
    if (job != NULL){
        printf("\n");
        kill(-job->child1_pid, SIGTSTP);
        stop_job(job->child1_pid);
    }
    else{
        printf("\n# ");
        fflush(stdout);
    }
}

void sigchld_handler(int signo){
    int pid, status;
    pid = waitpid(WAIT_ANY, &status, WUNTRACED|WNOHANG);
    make_job_status_done(pid);
}


void setFileRedirection(struct process proc1){
    if(proc1.inRedir != NULL){
        int inRedir = open(proc1.inRedir, O_RDONLY);
        dup2(inRedir, 0);
        close(inRedir);
    }
    if(proc1.outRedir != NULL){
        int outRedir = open(proc1.outRedir, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
        dup2(outRedir, 1);
        close(outRedir);
    }
    if(proc1.errRedir != NULL){
        int errRedir = open(proc1.errRedir, O_WRONLY| O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWGRP);
        dup2(errRedir, 1);
        close(errRedir);
    }
}

void fgRun(){
    if(firstJob == NULL){
        return;
    }
    int pid_to_fg;
    struct job * cur = firstJob;
    pid_to_fg = find_last_undone();
    makefg(pid_to_fg);
    
    kill(pid_to_fg, SIGCONT);
    waitpid(pid_to_fg, &status, WUNTRACED);
    if (WIFEXITED(status)){
        delete_job(pid_to_fg);
    }
}

void bgRun(){
    if(firstJob == NULL){
        return;
    }
    int pid_to_fg;
    struct job * cur = firstJob;
    pid_to_fg = find_last_undone();
    makebg(pid_to_fg);
    
    kill(pid_to_fg, SIGCONT);
}


int main(void) {
    

    while(TRUE){
    /*----parsing command line----*/
        fflush(stdout);
        promptForInput();
        char line[1000] = "";
        getLine(line);
        char *job_str;
        job_str = strdup(line);
        
        int bg = FALSE;
        
        /*get the programs to execute*/
        char *proc1_str = NULL;
        char *proc2_str = NULL;
        parseProcesses(line, &proc1_str, &proc2_str);
        int pipe_procs = (proc2_str != NULL);
        
        /*parse the two programs into structs*/
        struct process proc1;
        struct process proc2;
        proc1 = parseSingleProcess(proc1_str);
        proc2 = parseSingleProcess(proc2_str);
        
        bg = checkBg(&proc1) | checkBg(&proc2);
        if(pipe_procs){
            if(pipe(pipefd)==-1){
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }
    /*----finished parsing command line----*/
        
        
        if (proc1.params[0] == NULL) {
            continue;
            printJobs(DONEJOBS);
        } 
        else if (strcmp(proc1.params[0], "jobs") == 0) {
            printJobs(ALLJOBS);
        }
        else if (strcmp(proc1.params[0], "fg") == 0) {
            fgRun();
        } 
        else if (strcmp(proc1.params[0], "bg") == 0) {
            bgRun();
        } 

        else{
            printJobs(DONEJOBS);
        }
        
        /*delete zombies*/
        while(delete_first_zombie() == 1);
        
        

        /*-------------------take care of processes-----------------*/
        child1_pid = fork();
            

        if(child1_pid > 0){
            /*parent*/
            /*----set up signal handlers----*/
            if(!pipe_procs){
                signal(SIGINT,  sigint_handler);  /* ctrl-c */
                signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
                if(bg) signal(SIGCHLD, sigchld_handler);  /* child done */
            }

            proc1.pid = child1_pid;

            if(pipe_procs){/*check if we need to pipe*/
                
                child2_pid = fork();
                if(child2_pid > 0){
                /*parent*/
                /*----set up signal handlers----*/
                    if(!pipe_procs){
                        signal(SIGINT,  sigint_handler);  /* ctrl-c */
                        signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
                        if(bg) signal(SIGCHLD, sigchld_handler);  /* child done */
                    }

                    proc2.pid = child2_pid;
                    
                    /*close the pipe in the parent*/
                    close(pipefd[0]); 
                    close(pipefd[1]);
                }
                else if (child2_pid < 0){
                    /*fork error*/
                    perror("yash");
                }
                else{
                    /*child 2*/

                    
                    /*take care of redirections*/
                    setFileRedirection(proc2);
                    
                    /*pipe initialization*/
                    setpgid(0,child1_pid);
                    
                    close(pipefd[1]);/*second child close write end*/
                    if(dup2(pipefd[0], STDIN_FILENO) != STDIN_FILENO){
                        fprintf(stderr,"child 2 dup2 stdin error");
                    }

                    /*execute the command*/
                    execvp(proc2.params[0], proc2.params);
                    exit(1);
                }
            }
            
            /*put all processes in a new job struct*/
           struct job *newJob = (struct job*)malloc(sizeof(struct job));
           strcpy(newJob->str, job_str);
           newJob->next = NULL;
           newJob->child1_pid = 0;
           newJob->child2_pid = 0;
           newJob->status = RUNNING;
           newJob->bg = bg;
           
           addProcToJob(newJob, proc1);
           
           if (pipe_procs){/*single process*/
               addProcToJob(newJob, proc2);
           }
           
            addJobToJobList(newJob);

            /*if foreground*/
            if(!bg){
                if(!pipe_procs){
                    pid = waitpid(child1_pid, &status, WUNTRACED);
                    if(WIFEXITED(status)){
                        make_job_status_done(pid);
                        delete_job(pid);
                    }
                }
                else{
                    /*wait either one process or two processes*/
                    pid = waitpid(child1_pid, &status, WUNTRACED);
                    if(WIFEXITED(status)){
                        make_job_status_done(pid);
                        delete_job(pid);
                    }
                    pid = waitpid(child2_pid, &status, WUNTRACED);
                }
            }
           
        }
        
        else if(child1_pid < 0){
            perror("yash");
        }
        else{
            /*child1*/
            
            setFileRedirection(proc1);

            /*pipe initialization*/
            if(pipe_procs){
                setsid();/*create process group*/
                close(pipefd[0]);/*first child close read end*/
                if(dup2(pipefd[1], STDOUT_FILENO) != STDOUT_FILENO){
                    fprintf(stderr,"child 1 dup2 stdout error");
                }
            }
            execvp(proc1.params[0], proc1.params);
            exit(1);
        }
    }
}
