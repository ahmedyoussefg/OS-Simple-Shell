#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>

#define MAX_LENGTH 500
void register_child_signal();
void reap_child_zombie();
void on_child_exit();
void write_to_log_file(char *log_msg);
void shell();
void setup_environment();
void execute_shell_builtin(char *args[], int counter);
void execute_command(char *args[], int background);
void removeQuotes(char *input);

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
        fflush(stdin); 
        fgets(input, sizeof(input), stdin);

        if (input[strlen(input) - 1] == '\n') {
            input[strlen(input) - 1] = '\0';
        }
        char *token = strtok(input, " ");
        int is_background=0;
        while (token != NULL) {
            if (strcmp(token,"&")==0){
                is_background=1;
            }
            removeQuotes(token);
            args[counter++]=token;
            token = strtok(NULL, " ");
        }
        args[counter++]=NULL;
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
        // switch(input_type):
        //     case shell_builtin:
        //         execute_shell_bultin();
        //     case executable_or_error:
        //         execute_command():
    }
    while(strcmp(args[0],"exit")!=0);
}

void execute_shell_builtin(char* args[], int counter){
    if(strcmp(args[0],"cd")==0){
        if(strcmp(args[1],"~")==0){
            args[1]= getenv("HOME");
        }
        chdir(args[1]);
    }
    else if (strcmp(args[0],"echo")==0){
        for (int i =1;i<counter-1;i++){
            printf("%s",args[i]);
        }
        printf("\n");
    }
    else if(strcmp(args[0],"export")==0){
        // handle export
    }
}
void execute_command(char *args[],int background){
    int pid = fork();
    if (pid==0){
        // child process
        if(execvp(args[0], args)==-1){
            printf("Error occured.");
        }
    }
    else if (!background) {
        // parent process
        int status;
        waitpid(pid,&status,0);
    }
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
    waitpid(getpid(),&status,0);
}
void write_to_log_file(char * log_msg){
    FILE *log_file=fopen("shell_logfile.txt", "a");
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
void removeQuotes(char *input) {
    char *src = input;
    char *dst = input;

    // Iterate through each character in the string
    while (*src != '\0') {
        // If the character is not a double quote, copy it to the destination
        if (*src != '"') {
            *dst = *src;
            dst++;
        }
        src++;
    }

    // Null-terminate the modified string
    *dst = '\0';
}