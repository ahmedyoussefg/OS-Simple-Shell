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
#define ANSI_COLOR_RED "\x1b[91m"
#define ANSI_COLOR_GREEN "\x1b[92m"
#define ANSI_COLOR_BLUE "\x1b[94m"
#define ANSI_COLOR_RESET "\x1b[0m"

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
void clear_terminal();
void evaluate_expression(char *args[], int counter);
char *getHashValue(char *key);
void print_prompt();

char *variables[MAX_LENGTH];
char *values[MAX_LENGTH];
int variables_count = 0;

int main()
{
    register_child_signal();
    setup_environment();
    shell();
}
 
/// @brief Function that controls the flow of the program
void shell()
{
    char *args[MAX_LENGTH];
    int counter = 0;
    clear_terminal();
    do
    {
        char input[MAX_LENGTH];
        counter = 0;
        print_prompt();
        fgets(input, sizeof(input), stdin);
        if (input[strlen(input) - 1] == '\n')
        {
            input[strlen(input) - 1] = '\0';
        }

        int is_background = 0;
        parse_input(input, args, &counter, &is_background);

        int shell_built_in = 0;
        //  To see current variables' values
        // for (int i=0;i<variables_count;i++){
        //     printf("Variable %s = %s\n", variables[i], values[i]);
        // }
        
        if (args[0] == NULL)
            continue;
        if (strcmp(args[0], "cd") == 0 || strcmp(args[0], "export") == 0 || strcmp(args[0], "echo") == 0)
        {
            shell_built_in = 1;
        }
        if (counter >= 3)
            evaluate_expression(args, counter);
        if (shell_built_in)
        {
            execute_shell_builtin(args, counter);
        }
        else
        {
            execute_command(args, is_background);
        }
    } while (1);
}
/// @brief Function replaces every $var with its value
///        if there is no variable with the name given, it leaves it as it is
/// @param args : the arguments given by the user
/// @param counter : length of args
void evaluate_expression(char *args[], int counter)
{
    char *expression = NULL;
    char *result = NULL;
    for (int i = 1; i < counter - 1; i++)
    {
        int length = strlen(args[i]);
        char *new_arg = (char *)malloc(MAX_LENGTH);
        if (new_arg == NULL)
        {
            fprintf(stderr, "Memory allocation failed\n");
            exit(EXIT_FAILURE);
        }
        new_arg[0] = '\0'; // Initialize the new argument as an empty string
        int flag = (args[i][0]==args[i][length-1]) && args[i][0]=='\"';
        for (int j = flag; j < length-flag; j++)
        {   
            if (args[i][j] == '$')
            {
                expression = (char *)malloc(MAX_LENGTH);
                if (expression == NULL)
                {
                    fprintf(stderr, "Memory allocation failed\n");
                    exit(1);
                }
                int k = 0;
                j++; // Move to the character after '$'
                while (args[i][j] != ' ' && args[i][j] != '\0' && (j < length-flag))
                {
                    expression[k++] = args[i][j++];
                }
                expression[k] = '\0';
                // Get the value of the variable
                result = getHashValue(expression);
                if (result != NULL)
                {
                    // Replace the variable in the expression with its value
                    strcat(new_arg, result); // Append the value to the new argument
                    strcat(new_arg, " ");
                    free(result); // Free the memory allocated by getHashValue
                }
                else
                {
                    // leave the variable unchanged
                    // Append the variable name to the new argument with "$" at the beginning
                    char temp[MAX_LENGTH];
                    snprintf(temp, sizeof(temp), "$%s ", expression);
                    strcat(new_arg, temp);
                }
                free(expression); // Free the memory allocated for the expression
            }
            else
            {
                char temp[2] = {args[i][j], '\0'};
                strcat(new_arg, temp);
            }
        }
        // Update args[i] with the new argument
        if (flag){
            char *quoted_new_arg = (char *)malloc(strlen(new_arg) + 3); // +3 for the two quotes and null terminator
            if (quoted_new_arg == NULL)
            {
                fprintf(stderr, "Memory allocation failed\n");
                exit(EXIT_FAILURE);
            }
            sprintf(quoted_new_arg, "\"%s\"", new_arg);
            strcpy(args[i],quoted_new_arg);
            free(quoted_new_arg);
        }
        else 
            strcpy(args[i],new_arg);
        free(new_arg);
    }
}
/// @brief Execute of the commands {cd,export,echo}
/// @param args : the arguments entered by the user
/// @param counter : length of args
void execute_shell_builtin(char *args[], int counter)
{
    if (strcmp(args[0], "cd") == 0)
    {
        if (counter == 2)
        { // case "cd"
            return;
        }
        char *path =(char *) malloc(MAX_PATH_LENGTH);
 
        if (args[1][0]=='\"')args[1]++;
        if (args[1][0] == '~') {
            // Replace ~ with $HOME
            char *home = getenv("HOME");
            if (home != NULL) {
                // Allocate memory for the new path
                path = (char *)malloc(strlen(home) + strlen(args[1]) + 1);
                if (path != NULL) {
                    // Concatenate $HOME with the rest of args[1]
                    strcpy(path, home);
                    strcat(path, args[1] + 1); // Skip ~ in args[1]
                } else {
                    fprintf(stderr, "Memory allocation failed\n");
                    exit(EXIT_FAILURE);
                }
            } else {
                fprintf(stderr, "HOME environment variable not set\n");
                exit(EXIT_FAILURE);
            }
        } else {
            strcpy(path, args[1]);
        }
        int len =strlen(path);
        if (path[len-1]=='\"')
            path[len-1]='\0';
        if (path[len-1]==' ')
            path[len-1]='\0';
        chdir(path);
    }
    else if (strcmp(args[0], "echo") == 0)
    {
        for (int i = 1; i < counter - 1; i++)
        {
            if (args[i][0] == '\"')
            {
                int length = strlen(args[i]);
                for (int j = 1; j < length - 1; j++)
                {
                    printf("%c", args[i][j]);
                }
            }
        }
        printf("\n");
    }
    else if (strcmp(args[0], "export") == 0)
    {
        if (counter == 2)
            return;
        char *assignment = (char *)malloc(MAX_LENGTH);
        if (assignment != NULL)
        {
            strcpy(assignment, args[1]);
        }
        else
        {
            // Memory allocation failed
            fprintf(stderr, "Error in Memory Allocation\n");
            return;
        }
        char *equal_sign = strchr(assignment, '=');

        if (equal_sign != NULL)
        {
            *equal_sign = '\0'; // Replace '=' with null terminator
            variables[variables_count] = assignment;
            values[variables_count] = equal_sign + 1; // Move to the character after '='

            // Check and skip leading double quote
            if (*values[variables_count] == '\"')
            {
                values[variables_count]++;
            }

            // Check and skip trailing double quote
            size_t rhs_length = strlen(values[variables_count]);
            if (rhs_length > 0 && values[variables_count][rhs_length - 1] == '\"')
            {
                values[variables_count][rhs_length - 1] = '\0'; // Remove the trailing double quote
            }
        }
        else
        {
            variables[variables_count] = ""; // No '=' found, set an empty string for LHS
            values[variables_count] = "";    // Set an empty string for RHS
        }

        variables_count++;
    }
}
/// @brief Function executes non-builtin shell commands
/// @param args   : The arguments input by user
/// @param background  : boolean =1 if the commanad should be executed in background, 0 instead
void execute_command(char *args[], int background)
{
    if (strcmp(args[0], "exit") == 0)
    {
        exit(0);
        return;
    }
    int pid = fork();
    if (pid == 0)
    {
        // child process

        // Allocate memory for args2
        char *args2[MAX_LENGTH]; 
        int args2_index = 0;

        // Tokenize each argument and add tokens to args2
        for (int i = 0; args[i] != NULL; ++i)
        {
            char *token = strtok(args[i], " ");
            while (token != NULL)
            {
                args2[args2_index++] = strdup(token);
                token = strtok(NULL, " ");
            }
        }

        // Null-terminate args2
        args2[args2_index] = NULL;

        // Execute the command with the new arguments
        if (execvp(args2[0], args2) == -1)
        {
            printf(ANSI_COLOR_RED "Error occurred.\n" ANSI_COLOR_RESET);
            exit(1);
        }
    }
    else if (!background)
    {
        // parent process
        int status;
        waitpid(pid, &status, 0);
    }
}
/// @brief Function parses the input by the user and splits them into array of args
/// @param input : the string input by the user
/// @param args : the array being built
/// @param counter : it is length of args, so initially it's = zero
/// @param is_background the function also checks if the command entered by the user 
///                      should be executed in background or not
void parse_input(char *input, char *args[], int *counter, int *is_background)
{
    char *token = input;
    int in_quotes = 0;

    while (*token != '\0')
    {
        // Skip leading spaces
        while (*token == ' ')
        {
            token++;
        }

        if (*token == '\"')
        {
            // Start of quoted argument
            in_quotes = 1;
            args[*counter] = token;
            token++;
        }
        else
        {
            // Start of unquoted argument
            args[*counter] = token;
        }

        // Find the end of the argument
        while (*token != '\0' && ((*token != ' ' && !in_quotes) || in_quotes))
        {
            if (*token == '\"' && in_quotes)
            {
                in_quotes = 0;
                token++;
                break;
            }
            if (*token == '\"')
            {
                in_quotes = 1;
                token++;
            }
            if (*token == '&')
            {
                *is_background = 1;
                token++;
                continue;
            }
            token++;
        }

        // Null-terminate the argument
        if (*token != '\0')
        {
            *token = '\0';
            token++;
        }
        if (strcmp(args[*counter], "&") == 0)
        {
            args[*counter] = "";
            (*counter)--;
        }
        // Move to the next argument
        (*counter)++;
    }
    args[(*counter)++] = NULL;
}
/// @brief Function setups the environment for user. Currently it only makes the entry point be $HOME
void setup_environment()
{
    // go to home directory
    char *home = getenv("HOME");
    if (home != NULL)
    {
        chdir(home);
    }
    else
        chdir(".");
}
/// @brief Function kills zombie children by calling waitpid on them
void reap_child_zombie()
{
    int status;
    waitpid(-1, &status, WNOHANG);
}
/// @brief Function writes (appends) the log msg to file in $HOME
/// @param log_msg : The message to be writted
void write_to_log_file(char *log_msg)
{
    char *home = getenv("HOME");
    char path[256];
    strcpy(path, home);
    strcat(path, "/shell_logfile.txt");
    FILE *log_file = fopen(path, "a");
    fprintf(log_file, "%s", log_msg);
    fclose(log_file);
}
/// @brief Interrupt handler executed when child exit (SIGCHLD signal), it calls reap child zombie and write to log file
void on_child_exit()
{
    reap_child_zombie();
    write_to_log_file("Child terminated\n");
}
/// @brief Function that registers the interrupt with signal SIGCHLD, it calls back on_child_exit if interrupt occured
void register_child_signal()
{
    signal(SIGCHLD, on_child_exit);
}
/// @brief Function defines the style of prompt to input
void print_prompt()
{
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
/// @brief Function replaces /home/{USER} with ~
/// @param cwd : current working directory string
void replace_home_with_tilde(char *cwd)
{
    char *home = getenv("HOME"); // Get the home directory from the environment

    // Check if the home directory is a prefix of the current working directory
    if (strncmp(cwd, home, strlen(home)) == 0)
    {
        // Replace the home directory with ~ in the current working directory
        memmove(cwd, "~", 1);
        memmove(cwd + 1, cwd + strlen(home), strlen(cwd) - strlen(home) + 1);
    }
};
/// @brief Function clears terminal
void clear_terminal()
{
    printf("\033[2J"); // ANSI escape code for clearing the screen
    printf("\033[H");  // Move the cursor to the home position
}
/// @brief function returns hash value of stored variable, else returns NULL.
char *getHashValue(char *key)
{
    for (int i = 0; i < variables_count; i++)
    {
        if (strcmp(key, variables[i]) == 0)
        {
            char *ret = (char *)malloc(strlen(values[i]) + 1);
            if (ret != NULL)
            {
                strcpy(ret, values[i]);
                return ret;
            }
            else
            {
                // Memory allocation failed
                return NULL;
            }
        }
    }
    // variable not found
    return NULL;
}