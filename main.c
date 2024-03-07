#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>

#define MAX_LENGTH 500
#define MAX_PATH_LENGTH 1024
#define MAX_USERNAME_LENGTH 256
#define MAX_HOSTNAME_LENGTH 256
#define ANSI_COLOR_RED     "\x1b[91m"
#define ANSI_COLOR_GREEN   "\x1b[92m"
#define ANSI_COLOR_BLUE    "\x1b[94m"
#define ANSI_COLOR_RESET   "\x1b[0m"

void print_prompt();
void register_child_signal();
void reap_child_zombie();
void on_child_exit();
void write_to_log_file(char *log_msg);
void shell();
void setup_environment();
void execute_shell_builtin(char *args[], int counter);
void execute_command(char *args[], int background);
void parse_input(char *input, char *args[], int *counter, int *is_background);
void replace_home_with_tilde(char *cwd);
/*
TODO:
-Finish export method
-Finish evaluate_expression method
-Refactor code
*/
int main(){
    register_child_signal();
    setup_environment();
    shell();
}

void shell() {
    char *command="";
    char *args[MAX_LENGTH];
    int counter =0;
    do {
        char input[MAX_LENGTH];
        counter=0;
        print_prompt();
        fgets(input, sizeof(input), stdin);
        if (input[strlen(input) - 1] == '\n') {
            input[strlen(input) - 1] = '\0';
        }
        
        int is_background = 0;
        parse_input(input, args, &counter, &is_background);

        int shell_built_in=0;

        if(strcmp(args[0],"cd")==0||strcmp(args[0],"export")==0 || strcmp(args[0],"echo")==0){
            shell_built_in=1;
        }
        
        // evaluate_expression():
        if (shell_built_in){
            execute_shell_builtin(args, counter);
        }
        else {
            execute_command(args,is_background);
        }
    }
    while(strcmp(args[0],"exit")!=0);
}

void execute_shell_builtin(char* args[], int counter){
    if(strcmp(args[0],"cd")==0){
        if (counter ==2 ) { // case "cd"
            return;
        }
        if(strcmp(args[1],"~")==0){
            args[1]= getenv("HOME");
        }
        chdir(args[1]);
    }
    else if (strcmp(args[0],"echo")==0){
        for (int i =1;i<counter-1;i++){
            if (args[i][0]=='\"'){
                int length=strlen(args[i]);
                for (int j=1;j<length-1;j++){
                    printf("%c",args[i][j]);
                }
            }
        }
        printf("\n");
    }
    else if(strcmp(args[0],"export")==0){
        // handle export
    }
}
void execute_command(char *args[],int background){
    if (strcmp(args[0],"exit")==0){
        exit(0);
        return;
    }
    int pid = fork();
    if (pid==0){
        // child process
        if(execvp(args[0], args)==-1){
            printf("Error occured.\n");
        }
    }
    else if (!background) {
        // parent process
        int status;
        waitpid(pid,&status,0);
    }
}
void parse_input(char *input, char *args[], int *counter, int *is_background) {
    char *token = input;
    int in_quotes = 0;

    while (*token != '\0') {
        // Skip leading spaces
        while (*token == ' ') {
            token++;
        }

        if (*token == '\"') {
            // Start of quoted argument
            in_quotes = 1;
            args[*counter] = token;
            token++;
        } else {
            // Start of unquoted argument
            args[*counter] = token;
        }

        // Find the end of the argument
        while (*token != '\0' && ((*token != ' ' && !in_quotes) || in_quotes)) {
            if(*token == '\"' && in_quotes){
                in_quotes=0;
                token++;
                break;
            }
            if (*token=='&'){
                *is_background=1;
                token++;
                continue;
            }
            token++;
        }

        // Null-terminate the argument
        if (*token != '\0') {
            *token = '\0';
            token++;
        }
        if (strcmp(args[*counter],"&")==0){
            args[*counter]="";
            (*counter)--;
        }
        // Move to the next argument
        (*counter)++;

    }
    args[(*counter)++]=NULL;
}
void setup_environment(){
    // go to home directory
    char *home = getenv("HOME");
    if (home != NULL){
        chdir(home);
    }
    else
        chdir(".");
}
void reap_child_zombie(){
    int status;
    waitpid(-1, &status, WNOHANG);
}
void write_to_log_file(char * log_msg){
    char *home = getenv("HOME");
    char path[256];
    strcpy(path, home);
    strcat(path, "/shell_logfile.txt");
    FILE *log_file=fopen(path, "a");
    fprintf(log_file,"%s",log_msg);
    fclose(log_file);
}
void on_child_exit(){
    reap_child_zombie();
    write_to_log_file("Child terminated\n");
}
void register_child_signal(){
    signal(SIGCHLD, on_child_exit);
}
void print_prompt() {
    char username[MAX_USERNAME_LENGTH];
    char hostname[MAX_HOSTNAME_LENGTH];
    char cwd[MAX_PATH_LENGTH];

    // Get username
    getlogin_r(username, MAX_USERNAME_LENGTH);

    // Get hostname
    gethostname(hostname, MAX_HOSTNAME_LENGTH);

    // Get current working directory
    getcwd(cwd, sizeof(cwd));
    replace_home_with_tilde(cwd);
    // Print the stylish prompt
    printf(ANSI_COLOR_RED "[%s] " ANSI_COLOR_GREEN "%s@%s:" ANSI_COLOR_BLUE "%s" ANSI_COLOR_RESET "$ ",
           "SimpleShell", username, hostname, cwd);
    fflush(stdout);
}
void replace_home_with_tilde(char *cwd) {
    char *home = getenv("HOME");  // Get the home directory from the environment

    // Check if the home directory is a prefix of the current working directory
    if (strncmp(cwd, home, strlen(home)) == 0) {
        // Replace the home directory with ~ in the current working directory
        memmove(cwd, "~", 1);
        memmove(cwd + 1, cwd + strlen(home), strlen(cwd) - strlen(home) + 1);
    }
};