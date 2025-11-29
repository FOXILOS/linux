#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pwd.h>

#include <readline/readline.h>
#include <readline/history.h>

#define HISTORY_FILE ".kubsh_history"

sig_atomic_t signal_received = 0;

void echo(char *line) {
    printf("%s\n", line);
}

void sig_handler(int signum) {
    signal_received = signum;
    printf("Configuration reloaded\n");
}

void disk_info(char* device) {
    printf("Disk information for %s: \n", device);
    char command[256];
    snprintf(command, sizeof(command), "fdisk -l %s 2>/dev/null", device);
    int result = system(command);
    if (result != 0) {
        printf("Error: Can not get disk information for %s\n", device);
    }
}

void print_env(char* var_name) {
    char* value = getenv(var_name);
    if (value == NULL) {
        printf("Env variable %s not found\n", var_name);
        return;
    }
    printf("Env variable %s:\n", var_name);
    char* copy = strdup(value);
    char* token = strtok(copy, ":");
    int count = 1;
    while (token != NULL) {
        printf("%d: %s\n", count++, token);
        token = strtok(NULL, ":");
    }
    free(copy);
}

// Функции для VFS
void setup_users_vfs() {
    system("mkdir -p /home/user/users");
    printf("Users VFS directory created at /home/user/users/\n");
}

void refresh_users_vfs() {
    // Очищаем и пересоздаем структуру
    system("rm -rf /home/user/users/");
    
    struct passwd *pwd;
    setpwent();
    
    while ((pwd = getpwent()) != NULL) {
        if (pwd->pw_uid >= 1000) { // Только обычные пользователи
            char command[512];
            // Создаем каталог пользователя
            snprintf(command, sizeof(command), "mkdir -p /home/user/users/%s", pwd->pw_name);
            system(command);
            
            // Создаем файлы информации
            snprintf(command, sizeof(command), "echo '%d' > /home/user/users/%s/id", pwd->pw_uid, pwd->pw_name);
            system(command);
            
            snprintf(command, sizeof(command), "echo '%s' > /home/user/users/%s/home", pwd->pw_dir, pwd->pw_name);
            system(command);
            
            snprintf(command, sizeof(command), "echo '%s' > /home/user/users/%s/shell", pwd->pw_shell, pwd->pw_name);
            system(command);
        }
    }
    endpwent();
    printf("Users VFS refreshed with current system users\n");
}

void create_user_vfs(const char *username) {
    if (username == NULL || strlen(username) == 0) {
        printf("Usage: \\adduser <username>\n");
        return;
    }

    // 1. Создаем пользователя в системе (без вопросов)
    char command[256];
    snprintf(command, sizeof(command), "sudo adduser --disabled-password --gecos '' %s", username);
    int result = system(command);

    if (result != 0) {
        printf("Error: could not create user '%s'\n", username);
        return;
    }

    printf("User '%s' created successfully in system\n", username);

    // 2. Создаем каталог в VFS
    snprintf(command, sizeof(command), "mkdir -p /home/user/users/%s", username);
    system(command);

    // 3. Создаем файлы информации
    struct passwd *pwd = getpwnam(username);
    if (pwd != NULL) {
        snprintf(command, sizeof(command), "echo '%d' > /home/user/users/%s/id", pwd->pw_uid, username);
        system(command);
        
        snprintf(command, sizeof(command), "echo '%s' > /home/user/users/%s/home", pwd->pw_dir, username);
        system(command);
        
        snprintf(command, sizeof(command), "echo '%s' > /home/user/users/%s/shell", pwd->pw_shell, username);
        system(command);
        
        printf("User '%s' added to VFS\n", username);
    }
}

void delete_user_vfs(const char *username) {
    if (username == NULL || strlen(username) == 0) {
        printf("Usage: \\userdel <username>\n");
        return;
    }

    // 1. Удаляем пользователя из системы
    char command[256];
    snprintf(command, sizeof(command), "sudo userdel -r %s", username);
    int result = system(command);

    if (result != 0) {
        printf("Error: could not delete user '%s'\n", username);
        return;
    }

    printf("User '%s' deleted successfully from system\n", username);

    // 2. Удаляем каталог из VFS
    snprintf(command, sizeof(command), "rm -rf /home/user/users/%s", username);
    system(command);
    
    printf("User '%s' removed from VFS\n", username);
}

int main() {
    signal(SIGHUP, sig_handler);
    read_history(HISTORY_FILE);

    // Автоматически создаем VFS при запуске
    setup_users_vfs();
    refresh_users_vfs();

    char *input;

    while (1) {
        input = readline("$ ");

        if (signal_received) {
            signal_received = 0;
            continue;
        }

        if (input == NULL) {
            printf("\nExit (Ctrl+D)\n");
            break;
        }

        add_history(input);

        if (strlen(input) == 0) {
            free(input);
            continue;
        }
        
        if (strcmp(input, "\\q") == 0) {
            printf("Exit\n");
            break;
        } else if (strncmp(input, "echo ", 5) == 0) {
            echo(&input[5]);
        } else if (strcmp(input, "\\l /dev/sda") == 0) {
            disk_info("/dev/sda");
        } else if (strncmp(input, "\\e $", 4) == 0) {
            char* var_name = input + 4;
            if (*var_name != '\0') {
                print_env(var_name);
            } else {
                printf("Usage: \\e $VARIABLE_NAME\n");
            }
        } else if (strncmp(input, "\\adduser ", 9) == 0) {
            char* username = input + 9;
            if (strlen(username) > 0) {
                create_user_vfs(username);
            } else {
                printf("Usage: \\adduser <username>\n");
            }
        } else if (strncmp(input, "\\userdel ", 9) == 0) {
            char* username = input + 9;
            if (strlen(username) > 0) {
                delete_user_vfs(username);
            } else {
                printf("Usage: \\userdel <username>\n");
            }
        } else if (strcmp(input, "\\vfs refresh") == 0) {
            refresh_users_vfs();
        } else if (strcmp(input, "\\vfs list") == 0) {
            printf("Users in VFS:\n");
            system("ls -la /home/user/users/");
        } else if (strcmp(input, "\\vfs info") == 0) {
            printf("VFS location: /home/user/users/\n");
            printf("Each user directory contains: id, home, shell files\n");
            printf("Available commands:\n");
            printf("  \\vfs list     - List all users\n");
            printf("  \\vfs refresh  - Refresh user list\n");
            printf("  \\vfs info     - Show this help\n");
            printf("  \\adduser <name> - Create new user\n");
            printf("  \\userdel <name> - Delete user\n");
        } else {
            // Если команда не распознана, выполняем как системную
            int ret = system(input);
            if (ret == -1) {
                printf("%s: command not found\n", input);
            }
        }

        free(input);
    }
    
    write_history(HISTORY_FILE);
    return 0;
}
