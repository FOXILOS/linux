#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "vfs.h"

#define HISTORY_FILE ".kubsh_history"

static volatile sig_atomic_t signal_received = 0;

// Обработчик сигнала
void sig_handler(int signum) {
    (void)signum;
    signal_received = 1;
    printf("Configuration reloaded\n");
    fflush(stdout);
}

// Убрать кавычки из строки
char* strip_quotes(const char* str) {
    if (!str || strlen(str) < 2) return strdup(str ? str : "");
    
    size_t len = strlen(str);
    if ((str[0] == '\'' && str[len-1] == '\'') ||
        (str[0] == '"' && str[len-1] == '"')) {
        return strndup(str + 1, len - 2);
    }
    return strdup(str);
}

// Команда echo/debug
void cmd_echo(const char* msg) {
    char* stripped = strip_quotes(msg);
    printf("%s\n", stripped);
    free(stripped);
    fflush(stdout);
}

// Вывод переменной окружения
void cmd_env(const char* var_name) {
    char* value = getenv(var_name);
    if (!value) {
        printf("Env variable %s not found\n", var_name);
        fflush(stdout);
        return;
    }
    
    printf("%s\n", value);
    fflush(stdout);
}

// Вывод PATH построчно
void cmd_env_path(void) {
    char* value = getenv("PATH");
    if (!value) {
        printf("Env variable PATH not found\n");
        fflush(stdout);
        return;
    }
    
    char* copy = strdup(value);
    char* token = strtok(copy, ":");
    while (token) {
        printf("%s\n", token);
        token = strtok(NULL, ":");
    }
    free(copy);
    fflush(stdout);
}

// Выполнение внешней команды с проверкой "command not found"
void exec_external(const char* cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        execlp("sh", "sh", "-c", cmd, NULL);
        perror("execlp");
        exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
            printf("%s: command not found\n", cmd);
            fflush(stdout);
        }
    } else {
        perror("fork");
    }
}

// Информация о диске
void disk_info(const char* device) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "fdisk -l %s 2>/dev/null", device);
    system(cmd);
}

int main(void) {
    signal(SIGHUP, sig_handler);
    using_history();
    read_history(HISTORY_FILE);
    
    rebuild_users_vfs();
    
    char* input;
    while (1) {
        if (signal_received) {
            signal_received = 0;
            rebuild_users_vfs();
        }
        
        input = readline("kubsh$ ");
        if (!input) {
            printf("\nExit (Ctrl+D)\n");
            break;
        }
        
        if (*input) add_history(input);
        
        if (strlen(input) == 0) {
            free(input);
            continue;
        }
        
        // Обработка команд
        if (strcmp(input, "\\q") == 0) {
            printf("Exit\n");
            free(input);
            break;
        }
        else if (strncmp(input, "echo ", 5) == 0) {
            cmd_echo(input + 5);
        }
        else if (strncmp(input, "debug ", 6) == 0) {
            cmd_echo(input + 6);
        }
        else if (strcmp(input, "\\l /dev/sda") == 0) {
            disk_info("/dev/sda");
        }
        else if (strcmp(input, "\\e $HOME") == 0) {
            cmd_env("HOME");
        }
        else if (strcmp(input, "\\e $PATH") == 0) {
            cmd_env_path();
        }
        else if (strncmp(input, "\\e $", 4) == 0) {
            cmd_env(input + 4);
        }
        else if (strcmp(input, "\\vfs list") == 0) {
            list_users_vfs();
        }
        else if (strcmp(input, "\\vfs refresh") == 0) {
            rebuild_users_vfs();
            printf("Users VFS refreshed at %s\n", get_users_dir());
            fflush(stdout);
        }
        else if (strcmp(input, "\\vfs info") == 0) {
            printf("VFS directory: %s\n", get_users_dir());
            printf("Commands:\n  \\vfs list\n  \\vfs refresh\n  \\vfs info\n");
            fflush(stdout);
        }
        else {
            exec_external(input);
        }
        
        free(input);
    }
    
    write_history(HISTORY_FILE);
    return 0;
}
