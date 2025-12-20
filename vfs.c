#include "vfs.h"
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <dirent.h>

#define USERS_DIR "/opt/users"

// Создание каталога VFS
void init_users_dir(void) {
    struct stat st = {0};
    if (stat(USERS_DIR, &st) == -1) {
        mkdir(USERS_DIR, 0755);
    }
}

// Создание файлов id, home, shell для пользователя
void create_user_files(const struct passwd* pwd) {
    if (!pwd) return;
    
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", USERS_DIR, pwd->pw_name);
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0755);
    }

    char file[512];
    
    // id
    snprintf(file, sizeof(file), "%s/id", path);
    FILE* f = fopen(file, "w");
    if (f) {
        fprintf(f, "%d\n", pwd->pw_uid);
        fclose(f);
    }
    
    // home
    snprintf(file, sizeof(file), "%s/home", path);
    f = fopen(file, "w");
    if (f) {
        fprintf(f, "%s\n", pwd->pw_dir);
        fclose(f);
    }
    
    // shell
    snprintf(file, sizeof(file), "%s/shell", path);
    f = fopen(file, "w");
    if (f) {
        fprintf(f, "%s\n", pwd->pw_shell);
        fclose(f);
    }
}

// Перестройка VFS
void rebuild_users_vfs(void) {
    init_users_dir();
    
    struct passwd* pwd;
    setpwent();
    while ((pwd = getpwent()) != NULL) {
        // Только пользователи с shell
        if (pwd->pw_shell && strstr(pwd->pw_shell, "sh")) {
            create_user_files(pwd);
        }
    }
    endpwent();
}

// Список пользователей в VFS
void list_users_vfs(void) {
    DIR* dir = opendir(USERS_DIR);
    if (!dir) return;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR &&
            strcmp(entry->d_name, ".") != 0 &&
            strcmp(entry->d_name, "..") != 0) {
            printf("%s\n", entry->d_name);
        }
    }
    closedir(dir);
}

// Получение пути к VFS
const char* get_users_dir(void) {
    return USERS_DIR;
}
