#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_INPUT 1024
#define MAX_ARGS 100
#define MAX_ENV_VARS 100

// Environment variable structure
typedef struct {
    char name[50];
    char value[200];
} EnvVar;

EnvVar env_vars[MAX_ENV_VARS];
int env_var_count = 0;

// Function prototypes
void handle_cd(char *path);
void handle_pwd();
void set_env_var(char *name, char *value);
void unset_env_var(char *name);
char *get_env_var(char *name);
void expand_variables(char *command);
void execute_command(char *command);
void parse_and_execute(char *input);

int main() {
    char input[MAX_INPUT];

    while (1) {
        printf("xsh# ");
        fflush(stdout);

        if (!fgets(input, MAX_INPUT, stdin)) break; //Error
        input[strcspn(input, "\n")] = 0; // Remove trailing newline

        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            break;
        }

        parse_and_execute(input);
    }

    return 0;
}

void handle_cd(char *path) {
    if (chdir(path) != 0) {
        perror("cd");
    }
}

void handle_pwd() {
    char cwd[MAX_INPUT];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("pwd");
    }
}

void set_env_var(char *name, char *value) {
    for (int i = 0; i < env_var_count; i++) {
        if (strcmp(env_vars[i].name, name) == 0) {
            strncpy(env_vars[i].value, value, sizeof(env_vars[i].value));
            return;
        }
    }

    if (env_var_count < MAX_ENV_VARS) {
        strncpy(env_vars[env_var_count].name, name, sizeof(env_vars[env_var_count].name));
        strncpy(env_vars[env_var_count].value, value, sizeof(env_vars[env_var_count].value));
        env_var_count++;
    }
}

void unset_env_var(char *name) {
    for (int i = 0; i < env_var_count; i++) {
        if (strcmp(env_vars[i].name, name) == 0) {
            for (int j = i; j < env_var_count - 1; j++) {
                env_vars[j] = env_vars[j + 1];
            }
            env_var_count--;
            return;
        }
    }
}

char *get_env_var(char *name) {
    for (int i = 0; i < env_var_count; i++) {
        if (strcmp(env_vars[i].name, name) == 0) {
            return env_vars[i].value;
        }
    }
    return NULL;
}

void expand_variables(char *command) {
    char expanded[MAX_INPUT] = "";
    char *token = strtok(command, " ");
    while (token != NULL) {
        if (token[0] == '$') {
            char *value = get_env_var(token + 1);
            if (value) {
                strcat(expanded, value);
            }
        } else {
            strcat(expanded, token);
        }
        strcat(expanded, " ");
        token = strtok(NULL, " ");
    }
    strcpy(command, expanded);
}

void execute_command(char *command) {
    char *args[MAX_ARGS];
    char *token = strtok(command, " ");
    int arg_count = 0;

    while (token != NULL) {
        args[arg_count++] = token;
        token = strtok(NULL, " ");
    }
    args[arg_count] = NULL;

    if (fork() == 0) {
        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else {
        wait(NULL);
    }
}

void parse_and_execute(char *input) {
    char command[MAX_INPUT];
    strcpy(command, input);
    expand_variables(command);

    char *output_file = NULL;
    char *input_file = NULL;
    int background = 0;

    // Handle I/O redirection and background
    char *token = strtok(command, " ");
    char *args[MAX_ARGS];
    int arg_count = 0;

    while (token != NULL) {
        if (strcmp(token, ">") == 0) {
            output_file = strtok(NULL, " ");
        } else if (strcmp(token, "<") == 0) {
            input_file = strtok(NULL, " ");
        } else if (strcmp(token, "&") == 0) {
            background = 1;
        } else {
            args[arg_count++] = token;
        }
        token = strtok(NULL, " ");
    }
    args[arg_count] = NULL;

    if (strcmp(args[0], "cd") == 0) {
        handle_cd(args[1]);
    } else if (strcmp(args[0], "pwd") == 0) {
        handle_pwd();
    } else if (strcmp(args[0], "set") == 0) {
        set_env_var(args[1], args[2]);
    } else if (strcmp(args[0], "unset") == 0) {
        unset_env_var(args[1]);
    } else {
        // Handle piping, redirection, and background execution
        int fd_in = 0, fd_out = 1;

        if (input_file) {
            fd_in = open(input_file, O_RDONLY);
            if (fd_in < 0) {
                perror("open");
                return;
            }
            dup2(fd_in, 0);
        }
        if (output_file) {
            fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd_out < 0) {
                perror("open");
                return;
            }
            dup2(fd_out, 1);
        }

        if (background) {
            if (fork() == 0) {
                execvp(args[0], args);
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        } else {
            if (fork() == 0) {
                execvp(args[0], args);
                perror("execvp");
                exit(EXIT_FAILURE);
            } else {
                wait(NULL);
            }
        }

        if (fd_in != 0) close(fd_in);
        if (fd_out != 1) close(fd_out);
    }
}
