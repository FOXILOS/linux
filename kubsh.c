#define FUSE_USE_VERSION 31

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>

#include "vfs.h"

#include <readline/readline.h>
#include <readline/history.h>

#define HISTORY_FILE ".kubsh_history"

sig_atomic_t signal_received = 0;

/* ================= BASIC ================= */

void echo(char *line) {
    printf("%s\n", line);
}

void sig_handler(int signum) {
    (void)signum;
    signal_received = 1;
    int write_result __attribute__((unused)) = write(STDOUT_FILENO, "\nConfiguration reloaded\n", 24);
}

void fork_exec(char *full_path, char **argv) {
    int pid = fork();
    if (pid == 0) {
        execvp(full_path, argv);
        perror(full_path);
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
            printf("%s: command not found\n", full_path);
        }
    }
}

int is_executable(const char *path) {
    return access(path, X_OK) == 0;
}

/* ================= BUILTINS ================= */

void disk_info(char* device) {
    char command[256];
    snprintf(command, sizeof(command),
             "fdisk -l %s 2>/dev/null", device);
    int result __attribute__((unused)) = system(command);
}

void print_env(char* var_name) {
    char* value = getenv(var_name);
    if (!value) return;

    char* copy = strdup(value);
    if (!copy) return;

    char* token = strtok(copy, ":");
    while (token) {
        printf("%s\n", token);
        token = strtok(NULL, ":");
    }
    free(copy);
}

/* ================= VFS COMMANDS ================= */

// Функция для синхронизации VFS
void sync_vfs_users(void) {
    DIR *dir = opendir("/opt/users");
    if (!dir) {
        mkdir("/opt/users", 0755);
        dir = opendir("/opt/users");
        if (!dir) return;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        struct stat st;
        char path[512];
        snprintf(path, sizeof(path), "/opt/users/%s", entry->d_name);
        
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            // Проверяем, есть ли пользователь для этой директории
            struct passwd *pw = getpwnam(entry->d_name);
            if (!pw) {
                // Создаём пользователя
                char cmd[512];
                snprintf(cmd, sizeof(cmd), "useradd -m -d %s -s /bin/bash %s 2>/dev/null",
                         path, entry->d_name);
                int result __attribute__((unused)) = system(cmd);
                
                // Если не получилось через useradd, пробуем напрямую
                if (getpwnam(entry->d_name) == NULL) {
                    // Находим максимальный UID
                    uid_t max_uid = 1000;
                    struct passwd *p;
                    
                    setpwent();
                    while ((p = getpwent())) {
                        if (p->pw_uid > max_uid && p->pw_uid < 60000) {
                            max_uid = p->pw_uid;
                        }
                    }
                    endpwent();
                    
                    uid_t uid = max_uid + 1;
                    gid_t gid = uid;
                    
                    // Добавляем в /etc/passwd
                    FILE *f = fopen("/etc/passwd", "a");
                    if (f) {
                        fprintf(f, "%s:x:%d:%d:%s:%s:/bin/bash\n",
                                entry->d_name, uid, gid, entry->d_name, path);
                        fclose(f);
                    }
                    
                    // Добавляем в /etc/shadow
                    FILE *shadow = fopen("/etc/shadow", "a");
                    if (shadow) {
                        fprintf(shadow, "%s:*:0:0:99999:7:::\n", entry->d_name);
                        fclose(shadow);
                    }
                }
            }
        }
    }
    closedir(dir);
}

/* ================= MAIN ================= */

int main(void) {
    signal(SIGHUP, sig_handler);

    char history_path[512];
    snprintf(history_path, sizeof(history_path),
             "%s/%s", getenv("HOME"), HISTORY_FILE);

    read_history(history_path);

    /* VFS */
    mkdir("/opt/users", 0755);
    
    // Синхронизируем существующие директории перед запуском VFS
    sync_vfs_users();
    
    start_users_vfs("/opt/users");

    char *input;

    while (1) {
        input = readline("$ ");

        if (signal_received) {
            signal_received = 0;
            free(input);
            continue;
        }

        if (input == NULL) {
            printf("\nExit (Ctrl+D)\n");
            break;
        }

        if (*input)
            add_history(input);

        if (strcmp(input, "\\q") == 0) {
            printf("Exit\n");
            free(input);
            break;
        }
        else if (strncmp(input, "debug ", 6) == 0) {
            char* temp = input + 6;
            if (temp[0] == '\'' && temp[strlen(temp) - 1] == '\'') {
                temp[strlen(temp) - 1] = '\0';
                temp++;
            }
            echo(temp);
        }
        else if (!strcmp(input, "\\l /dev/sda")) {
            disk_info("/dev/sda");
        }
        else if (!strncmp(input, "\\e $", 4))  {
            print_env(input + 4);
        }
        // Команда для обновления VFS
        else if (!strcmp(input, "\\vfs refresh") || !strcmp(input, "\\vfs sync")) {
            printf("Синхронизация VFS...\n");
            sync_vfs_users();
            printf("VFS синхронизирована\n");
        }
        // Команда для списка пользователей VFS
        else if (!strcmp(input, "\\vfs list")) {
            DIR *dir = opendir("/opt/users");
            if (!dir) {
                printf("Не удалось открыть /opt/users\n");
            } else {
                struct dirent *entry;
                while ((entry = readdir(dir)) != NULL) {
                    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                        printf("%s\n", entry->d_name);
                    }
                }
                closedir(dir);
            }
        }
        else {
            char *argv[64];
            int argc = 0;

            char *token = strtok(input, " ");
            while (token && argc < 63) {
                argv[argc++] = token;
                token = strtok(NULL, " ");
            }
            argv[argc] = NULL;

            if (argv[0])
                fork_exec(argv[0], argv);
        }

        free(input);
    }

    write_history(history_path);
    stop_users_vfs();
    return 0;
}
