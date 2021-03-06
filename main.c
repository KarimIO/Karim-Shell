// ========================================
// Author:  Karim Abdel Hamid (900 143 463)
// Project: KArim SHell (KASH)
// File:    main.c
// License: Unlicense
// ========================================

#include <string.h>
#include <stdio.h>      // Printf
#include <sys/wait.h>   // Wait
#include <unistd.h>     // Fork
#include <stdlib.h>
#include <signal.h>
#include <regex.h>
#include <stdbool.h>

const int kBufferSize = 1024;
const int kMaxLine = 8;
unsigned int kMaxHistory = 10;
unsigned int numHistory = 0;
pid_t process;
char cwd[1024];
char login[128];
char computer_name[128];

#define GRN   "\x1B[32m"
#define BLU   "\x1B[34m"
#define RESET "\x1B[0m"

#define BOLD "\033[32;1m"
#define REGULAR "\033[0m"

void printPrompt() {
    printf("kash " BOLD GRN "%s@%s: " BLU "(%s)" RESET " > ", login, computer_name, cwd);
    fflush(stdout);
}

char *readLine() {
    unsigned int bufferSize = kBufferSize;
    char *buffer = (char *)malloc(bufferSize);
    if (buffer == 0) {
        printf("Could not create buffer of size %i.\n", bufferSize);
        exit(-1);
    }

    char c = getchar();
    unsigned short i = 0;
    for(; c != '\n' && c != EOF; ++i) {
        buffer[i] = c;
        
        if (i > bufferSize) {
            bufferSize += kBufferSize;
            buffer = (char *)realloc(buffer, bufferSize);
            if (buffer == 0) {
                printf("Could not create buffer of size %i.\n", bufferSize);
                exit(-1);
            }

            printf("Realloced to %i\n", bufferSize);
        }

        c = getchar();
    }

    buffer = (char *)realloc(buffer, (i + 1));
    buffer[i] = '\0';
    return buffer;
}

char **parseLine(char *line) {
    unsigned int argc = kMaxLine;
    char **args = (char **)malloc(sizeof(char *) * argc);
    if (args == 0) {
        printf("Could not create buffer of size %i.\n", argc);
        exit(-1);
    }
    char *ptr;
    ptr = strtok(line, " \t\r\n\a");
    
    int i = 0;
    args[i] = ptr;
    while(ptr != NULL) {
        if (i >= argc) {
            argc += kMaxLine;
            args = (char **)realloc(args, sizeof(char *) * argc);
            if (args == 0) {
                printf("Could not create buffer of size %i.\n", argc);
                exit(-1);
            }
        }

        ++i;
        ptr = strtok(NULL, " \t\r\n\a");
        args[i] = ptr;
    };

    return args;
}

void exitHandler(int sig) {
    if (process != 0) {
        kill(process, SIGINT);
        process = 0;
    }
    else {
        signal(sig, SIG_IGN);
        printf("\n");
        printPrompt();
        fflush(stdout);
    }
}

void printCommand(char **args) {
    if (args[0] != NULL) {
        printf(BOLD "%s " REGULAR , args[0]);
    
        int i = 1;
        while (args[i] != NULL) {
            printf("%s " , args[i++]);
        }
    }
}

void execute(char **args) {
    unsigned int i = 0;
    for (; args[i] != NULL; ++i);
        
    bool background_process = false;
    if (strcmp(args[i-1], "&") == 0) {
        background_process  = true;
        args[i-1] = NULL;
    }
    process = fork();
    if (process == 0) {
        if (execvp(args[0], args) == -1) {
            printf("%s: command not found.\n", args[0]);
            exit(-1);
        }
    }
    else if (process < 0) {
        printf("Error\n");
        process = 0;
    }
    else {
        if (background_process) {
            printf("Running process [%i] in the background.\n", process);
        }
        else {
            wait(NULL);
        }

        process = 0;
    }
}

void specprint(const char *str) {
    printf("%s\n", str);

    fflush(stdin);
    fflush(stdout);
}

void handleHistory(char ***args) {
    if (args[kMaxHistory-1] != NULL) {
        free(args[kMaxHistory-1]);
    }

    for (int i = kMaxHistory-2; i >= 0; --i) {
        args[i+1] = args[i];
    }
    
    numHistory++;
}

int max(int a, int b) {
    return (a > b) ? a : b;
}

void printHistory(char ***args) {
    if (args[0] == NULL) {
        printf("No available history. Please write a command before attempting history.\n");
    }
    else {
        for (int i = kMaxHistory-1; i >= 0; --i) {
            if (args[i] != NULL) {
                int m = max(numHistory - kMaxHistory, 0) + i + 1;
                printf("%i: ", m);
                printCommand(args[i]);
                printf("\n");
            }
        }
    }
}

void setCWD() {
    char temp[1024];
    if (getcwd(temp, sizeof(temp)) == NULL) {
        strcpy(cwd, "[unknown path]");
    }
    else {
        // Replace /home/x or /users/x with ~
        const char *home = getenv("HOME");
        size_t homelen = strlen(home);
        if (strncmp(home, temp, homelen) == 0) {
            cwd[0] = '~';
            strcpy(&cwd[1], &temp[homelen]);
        }
        else {
            strcpy(cwd, temp);
        }
    }
}

void setUSR() {
    const char *temp = getenv("USER");
    if (temp == NULL) {
        if (getlogin_r(login, 128) != 0) {
            strcpy(login, "[unknown user]");
        }
    }
    else {
        strcpy(login, temp);
    }

    fseek(stdin,0,SEEK_END);
}

void checkSudo() {
    uid_t uid = geteuid();
    if (uid == 0) {
        strcpy(login, "sudo");
    }
}

void setComputerName() {
    const char *temp = getenv("HOSTNAME");
    if (temp == NULL) {
        if (gethostname(computer_name, 128) != 0) {
            strcpy(computer_name, "computer");
        }
    }
    else {
        strcpy(computer_name, temp);
    }

}

int main(int argc, char *argv[]) {
    process = 0;
    printf("Welcome to the " BOLD "KArim SHell" REGULAR "(kash).\n\tUse 'help' for details.\n");
    signal(SIGINT, exitHandler);

    // If we supply an extra parameter, set the max history to it, with a minimum value of 5.
    if (argc > 1) {
        int val = atoi(argv[1]);
        if (val < 5) {
            kMaxHistory = 5;
            printf("\tMax history value is invalid! Cannot be less than 5. Setting to 5.\n");
        }
        else {
            kMaxHistory = val;
            printf("\tMax history set to: %u\n", kMaxHistory);
        }
    }

    unsigned int *numCommands = (unsigned int *)malloc(sizeof(unsigned int) * kMaxHistory);
    char ***args = malloc(sizeof(char **) * kMaxHistory);
    char   *line;
    bool    looping = true;

    setCWD();
    setUSR();
    setComputerName();
    checkSudo();

    fflush(stdin);
    fflush(stdout);

    do {
        printPrompt();

        line = readLine();
        fflush(stdin);
        fflush(stdout);
        if (strcmp(line, "exit") == 0) {
            looping = false;
        }
        else if (strcmp(line, "history") == 0) {
            printHistory(args);
        }
        else if (strcmp(line, "help") == 0) {
            printf("Type the following commands for help:\n");
            printf("\thelp:\t\tGet help.\n");
            printf("\tcd:\t\tChange directory.\n");
            printf("\t!!:\t\tRun last command.\n");
            printf("\t!n:\t\tRun command [n]. See 'history' for the command this refers to.\n");
            printf("\thistory:\tShows all commands.\n");
            printf("\texit:\t\tExit program.\n");

            handleHistory(args);
            args[0] = parseLine(line);
        }
        else if (strlen(line) >= 2 && line[0] == 'c' && line[1] == 'd') {
            if (strlen(line) < 4)
                printf("A path must be provided!\n\tUsage: cd [path]\n");
            else if (strlen(line) > 1026) {
                printf("The path provided to cd is too big!\n");
            }
            else {
                chdir(&line[3]);
                setCWD();
            }

            handleHistory(args);
            args[0] = parseLine(line);
        }
        else if (line[0] == '!') {
            if (line[1] == '!') {
                if (args[0] != NULL) {
                    printCommand(args[0]);
                    printf("\n");
                    fflush(stdout);
                    execute(args[0]);
                }
                else {
                    printf("No commands in history! Please insert a command before attempting this.\n");
                }
            }
            else {
                int v = atoi(&line[1]);
                int min = max(numHistory - kMaxHistory, 0) + 1;
                int max = numHistory;
                if (v < min || v > max) {
                    printf("No such command in history!\n");
                    if (numHistory == 0)
                        printf("\tPlease insert a command before attempting this.\n");
                    else
                        printf("\tPlease use a value from !%i to !%i.\n", min, max);
                }
                else if (args[v-1] != NULL) {
                    v -= min;
                    printCommand(args[v]);
                    printf("\n");
                    fflush(stdout);
                    execute(args[v]);
                    checkSudo();
                }
            }
        }
        else {
            handleHistory(args);
            args[0] = parseLine(line);
            execute(args[0]);
        }
        fflush(stdin);
        fflush(stdout);
    } while(looping);

    return 0;
}
