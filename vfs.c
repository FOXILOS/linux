#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/stat.h>

#define MOUNT_POINT "/home/user/users"

// Простая эмуляция VFS через реальные каталоги
void create_user_directory(const char *username) {
    char path[256];
    
    // Создаем каталог пользователя
    snprintf(path, sizeof(path), "%s/%s", MOUNT_POINT, username);
    mkdir(path, 0755);
    
    // Получаем информацию о пользователе
    struct passwd *pwd = getpwnam(username);
    if (pwd != NULL) {
        // Создаем файл id
        snprintf(path, sizeof(path), "%s/%s/id", MOUNT_POINT, username);
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "%d\n", pwd->pw_uid);
            fclose(f);
        }
        
        // Создаем файл home
        snprintf(path, sizeof(path), "%s/%s/home", MOUNT_POINT, username);
        f = fopen(path, "w");
        if (f) {
            fprintf(f, "%s\n", pwd->pw_dir);
            fclose(f);
        }
        
        // Создаем файл shell
        snprintf(path, sizeof(path), "%s/%s/shell", MOUNT_POINT, username);
        f = fopen(path, "w");
        if (f) {
            fprintf(f, "%s\n", pwd->pw_shell);
            fclose(f);
        }
        
        printf("Created VFS directory for user: %s\n", username);
    } else {
        printf("User %s not found in system\n", username);
    }
}

void delete_user_directory(const char *username) {
    char command[256];
    snprintf(command, sizeof(command), "rm -rf %s/%s", MOUNT_POINT, username);
    system(command);
    printf("Deleted VFS directory for user: %s\n", username);
}

void list_users() {
    printf("Users in VFS:\n");
    char command[256];
    snprintf(command, sizeof(command), "ls -la %s/", MOUNT_POINT);
    system(command);
}

void setup_vfs() {
    // Создаем основную директорию
    char command[256];
    snprintf(command, sizeof(command), "mkdir -p %s", MOUNT_POINT);
    system(command);
    printf("VFS setup completed at: %s\n", MOUNT_POINT);
}

int main() {
    printf("Simple VFS Manager\n");
    printf("==================\n");
    
    // Создаем базовую структуру
    setup_vfs();
    
    // Создаем каталоги для существующих пользователей
    struct passwd *pwd;
    setpwent();
    
    while ((pwd = getpwent()) != NULL) {
        if (pwd->pw_uid >= 1000) { // Только обычные пользователи
            create_user_directory(pwd->pw_name);
        }
    }
    endpwent();
    
    printf("\nVFS is ready! Directory: %s\n", MOUNT_POINT);
    printf("You can now use:\n");
    printf("  ls %s/          # List users\n", MOUNT_POINT);
    printf("  cat %s/username/id    # View user ID\n", MOUNT_POINT);
    printf("  cat %s/username/home  # View home directory\n", MOUNT_POINT);
    printf("  cat %s/username/shell # View shell\n", MOUNT_POINT);
    
    return 0;
}
