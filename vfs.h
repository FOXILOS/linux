#ifndef VFS_H
#define VFS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <dirent.h>

#define USERS_DIR "/opt/users"

void init_users_dir(void);
void rebuild_users_vfs(void);
void list_users_vfs(void);
const char* get_users_dir(void);
void create_user_files(const struct passwd* pwd);  // Изменено: принимает struct passwd*

#endif
