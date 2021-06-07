#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_LINE 80
#define MAX_ARGS (MAX_LINE/2 + 1)
#define REDIRECT_OUT_OP '>'
#define REDIRECT_IN_OP '<'
#define PIPE_OP '|'
#define BG_OP '&'

/* Holds a single command. */
typedef struct Cmd {
    /* The command as input by the user. */
    char line[MAX_LINE + 1];
    /* The command as null terminated tokens. */
    char tokenLine[MAX_LINE + 1];
    /* Pointers to each argument in tokenLine, non-arguments are NULL. */
    char* args[MAX_ARGS];
    /* Pointers to each symbol in tokenLine, non-symbols are NULL. */
    char* symbols[MAX_ARGS];
    /* The process id of the executing command. */
    pid_t pid;
    
    /* TODO: Additional fields may be helpful. */
    
    int done;
    int to_print;
    int stop;
    
} Cmd;

/* The process of the currently executing foreground command, or 0. */
pid_t foregroundPid = 0;

int curSize = 0;
int is_empty = 0;

/*
 To keep the background command jobs
 */
Cmd** jobs;

/*
 Save the prevvious command for CTR+Z usage
 */
Cmd* prev;

/* Parses the command string contained in cmd->line.
 * * Assumes all fields in cmd (except cmd->line) are initailized to zero.
 * * On return, all fields of cmd are appropriatly populated. */
void parseCmd(Cmd* cmd) {
    char* token;
    int i=0;
    strcpy(cmd->tokenLine, cmd->line);
    strtok(cmd->tokenLine, "\n");
    token = strtok(cmd->tokenLine, " ");
    while (token != NULL) {
        if (*token == '\n') {
            cmd->args[i] = NULL;
        } else if (*token == REDIRECT_OUT_OP || *token == REDIRECT_IN_OP
                   || *token == PIPE_OP || *token == BG_OP) {
            cmd->symbols[i] = token;
            cmd->args[i] = NULL;
        } else {
            cmd->args[i] = token;
        }
        token = strtok(NULL, " ");
        i++;
    }
    cmd->args[i] = NULL;
}

/* Finds the index of the first occurance of symbol in cmd->symbols.
 * * Returns -1 if not found. */
int findSymbol(Cmd* cmd, char symbol) {
    for (int i = 0; i < MAX_ARGS; i++) {
        if (cmd->symbols[i] && *cmd->symbols[i] == symbol) {
            return i;
        }
    }
    return -1;
}

/* Signal handler for SIGTSTP (SIGnal - Terminal SToP),
 * which is caused by the user pressing control+z. */
void sigtstpHandler(int sig_num) {
    /* Reset handler to catch next SIGTSTP. */
    signal(SIGTSTP, sigtstpHandler);
    if (foregroundPid > 0) {
        /* Foward SIGTSTP to the currently running foreground process. */
        kill(foregroundPid, SIGTSTP);
        /* TODO: Add foreground command to the list of jobs. */
        curSize++;
        jobs = (Cmd**) realloc(jobs, curSize * sizeof(Cmd*));
        jobs[curSize-1] = prev;
        jobs[curSize-1]->stop = 1;
    }
}

/*
 Execute the given command.
 The method takes in the Cmd pointer of the current command and a background indicator.
 If background = 1, execute it as background
 If backgorund = 0, execute it as foreground
 */
static void execute_cmd(Cmd *cmd, int background){
    pid_t pid = fork();
    switch(pid){
        case -1:            //PID Error
            perror("fork");
            break;
        case 0:                //Child PID
            if(findSymbol(cmd, REDIRECT_IN_OP) != -1){//Input from file.
                int file_direct = open(cmd->args[findSymbol(cmd, REDIRECT_IN_OP)+1], O_RDONLY);
                if(dup2(file_direct, STDIN_FILENO) != STDIN_FILENO){
                    perror("dup2");
                    exit(1);
                }
            }
            if(findSymbol(cmd, REDIRECT_OUT_OP) != -1){ //Output to file.
                int file_direct = open(cmd->args[findSymbol(cmd, REDIRECT_OUT_OP)+1], O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP|S_IWGRP);
                if(dup2(file_direct, STDOUT_FILENO) != STDOUT_FILENO){
                    perror("dup2");
                    exit(1);
                }
            }
            if(findSymbol(cmd, PIPE_OP) != -1 && background == 0){ //Pipes
                //Seperate the left and right args
                
                //Left args
                int pipe_position = findSymbol(cmd, PIPE_OP);
                char* args_left[pipe_position * sizeof(char *)];
                for (int i = 0; i < pipe_position; i++) {
                    args_left[i] = cmd->args[i];
                }
                
                //Right args
                int right_size = pipe_position + 1;
                while(cmd->args[right_size] != NULL){
                    right_size++;
                }
                char* args_right[right_size * sizeof(char *)];
                int args_pos = pipe_position + 1;
                for (int i = 0; i < right_size; i++) {
                    args_right[i] = cmd->args[args_pos];
                    args_pos++;
                }
                
                int fd[2];
                
                // Make the pipe for communication
                pipe(fd);
                
                // Fork a child process
                pid_t pid = fork();
                
                if (pid == -1) {
                    perror("fork");
                    exit(1);
                }
                
                if (pid == 0) {
                    // Child process
                    dup2(fd[0], 0);
                    
                    // Close the "write" end of the pipe for the child.
                    // Parent still has it open; child doesn't need it.
                    close(fd[1]);
                    execvp(args_right[0], args_right);
                    // We only get here if exec() fails
                    perror("execvp");
                    exit(1);
                } else {
                    // Parent process
                    dup2(fd[1], 1);
                    // Close the "read" end of the pipe for the parent.
                    // Child still has it open; parent doesn't need it.
                    close(fd[0]);
                    execvp(args_left[0], args_left);
                    
                    // We only get here if exec() fails
                    perror("execvp");
                    exit(1);
                }
                break;
            }
            execvp(cmd->args[0], cmd->args);
            perror("execvp");
            exit(0);
        default:             //Parent PID
            if(!background){
                prev = (Cmd*) malloc(1 * sizeof(Cmd));
                prev = cmd;
                foregroundPid = pid;
                waitpid(pid, NULL, WUNTRACED);
            } else {
                //Storing job
                if(setpgid(pid, 0) != 0) perror("setpgid");
                curSize++;
                jobs = (Cmd**) realloc(jobs, curSize * sizeof(Cmd*));
                jobs[curSize-1] = cmd;
                jobs[curSize-1]->line[strlen(jobs[curSize-1]->line) - 1] = '\0';
                jobs[curSize-1]->line[strlen(jobs[curSize-1]->line) - 1] = '\0';
                jobs[curSize-1]->pid = pid;
                jobs[curSize-1]->done = 0;
                jobs[curSize-1]->to_print = 0;
                jobs[curSize-1]->stop = 0;
            }
            break;
    }
}

/*
 bg_processes will check each background processes and update its status with a stdout.
 Process is done: [1] Done ls -la, [<job num>] Done <args>
 Process is kill: Terminated sleep 100 ,Terminated <commandArgs>
 Process is exited: Exit <exitCode> <commandArgs>
 */
void bg_processes(){
    int i = 0;
    while(i < curSize){
        int status;
        if(waitpid(jobs[i]->pid, &status, WNOHANG) > 0){ //job is terminated
            if(WIFEXITED(status)){
                status = WEXITSTATUS(status);
                jobs[i]->done = 1;
            }
            else if(WIFSIGNALED(status)){
                status = WTERMSIG(status);
                jobs[i]->done = 1;
                printf("[%d] Terminated\t", i+1);
                if(jobs[i]->to_print == 0){
                    printf("%s\n",jobs[i]->line);
                    jobs[i]->to_print = 1;
                }
                return;
            }
            
            if(status != 0){
                printf("[%d] Exit\t%d ", i+1, status);
            }
            else{
                printf("[%d] Done\t", i+1);
            }
            if(jobs[i]->to_print == 0){
                printf("%s\n",jobs[i]->line);
                jobs[i]->to_print = 1;
            }
        }
        i++;
    }
    //Resets the job is all the background jobs is done
    int count = 0;
    for (int i = 0; i < curSize; i++) {
        if(jobs[i]->to_print == 1){
            count++;
        }
    }
    if(count == curSize){
        curSize = 0;
        jobs = (Cmd**) realloc(jobs, curSize * sizeof(Cmd*));
    }
    return;
}

int main(void) {
    system("clear");
    jobs = (Cmd**)malloc(curSize * sizeof(Cmd*));
    /* Listen for control+z (suspend process). */
    signal(SIGTSTP, sigtstpHandler);
    while (1) {
        printf("352> ");
        fflush(stdout);
        Cmd *cmd = (Cmd*) calloc(1, sizeof(Cmd));
        fgets(cmd->line, MAX_LINE, stdin);
        parseCmd(cmd);
        bg_processes();
        if (!cmd->args[0]) {
            free(cmd);
        } else if (strcmp(cmd->args[0], "exit") == 0) {
            for(int i = 0; i < curSize; i++){
                free(jobs[i]);
            }
            free(jobs);
            free(cmd);
            exit(0);
            /* TODO: Add built-in commands: jobs and bg. */
        }
        else if(strcmp(cmd->args[0], "jobs") == 0){
            for(int i = 0; i < curSize; i++){
                if(jobs[i]->done == 0 && jobs[i]->to_print == 0 && jobs[i]->stop != 1)
                    printf("[%d] Running\t%s\n", i+1, jobs[i]->line);
                else if(jobs[i]->done == 1 && jobs[i]->to_print == 0 && jobs[i]->stop != 1){
                    printf("[%d] Done\t%s\n", i+1, jobs[i]->line);
                    jobs[i]->to_print = 1;
                }
                else if(jobs[i]->stop == 1)
                    printf("[%d] Stopped\t%s\n", i+1, jobs[i]->line);
            }
        }
        else if(strcmp(cmd->args[0], "bg") == 0){
            int jobPid = jobs[atoi(cmd->args[1])]->pid;
            kill(jobPid, SIGCONT);
            jobs[atoi(cmd->args[1])]->stop = 0;
        } else {
            if (findSymbol(cmd, BG_OP) != -1) {
                /* TODO: Run command in background. */
                execute_cmd(cmd, 1);
                printf("[%d] %d\n", curSize, jobs[curSize-1]->pid);
                
            } else {
                /* TODO: Run command in foreground. */
                execute_cmd(cmd, 0);
            }
        }
        /* TODO: Check on status of background processes. */
    }
    return 0;
}

