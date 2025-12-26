#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>

#include "vfs.h"

#define USERS_DIR "/opt/users"
#define PASSWD_FILE "/etc/passwd"

/* -------------------------------------------------- */

static int vfs_pid = -1;

user_info users[128];
int users_count = 0;

/* -------------------------------------------------- */

static int endswith(const char *s, const char *suf) {
    size_t a = strlen(s), b = strlen(suf);
    return a >= b && strcmp(s + a - b, suf) == 0;
}

/* -------------------------------------------------- */
/* ЧТЕНИЕ /etc/passwd */

static void load_users(void) {
    struct passwd *p;
    users_count = 0;

    setpwent();
    while ((p = getpwent()) && users_count < 128) {
        if (!endswith(p->pw_shell, "sh"))
            continue;

        users[users_count].name  = strdup(p->pw_name);
        users[users_count].uid   = p->pw_uid;
        users[users_count].home  = strdup(p->pw_dir);
        users[users_count].shell = strdup(p->pw_shell);
        users_count++;
    }
    endpwent();
}

static user_info *find_user(const char *name) {
    for (int i = 0; i < users_count; i++)
        if (strcmp(users[i].name, name) == 0)
            return &users[i];
    return NULL;
}

/* -------------------------------------------------- */
/* 🔥 ГЛАВНОЕ: синхронизация директорий → passwd */

static void sync_users_from_dirs(void) {
    DIR *d = opendir(USERS_DIR);
    if (!d)
        return;

    struct dirent *e;

    while ((e = readdir(d))) {
        if (e->d_name[0] == '.')
            continue;

        if (find_user(e->d_name))
            continue;

        uid_t max_uid = 1000;
        struct passwd *p;

        setpwent();
        while ((p = getpwent()))
            if (p->pw_uid > max_uid)
                max_uid = p->pw_uid;
        endpwent();

        uid_t uid = max_uid + 1;

        FILE *f = fopen(PASSWD_FILE, "a");
        if (!f)
            continue;

        fprintf(
            f,
            "%s:x:%d:%d:%s:/home/%s:/bin/bash\n",
            e->d_name, uid, uid, e->d_name, e->d_name
        );

        fflush(f);
        fsync(fileno(f));
        fclose(f);

        load_users();  // обновляем кеш
    }

    closedir(d);
}

/* -------------------------------------------------- */
/* FUSE */

int vfs_getattr(const char *path, struct stat *st, struct fuse_file_info *fi) {
    (void) fi;
    memset(st, 0, sizeof(*st));

    if (!strcmp(path, "/")) {
        st->st_mode = S_IFDIR | 0555;
        st->st_nlink = 2;
        return 0;
    }

    char user[128], file[128];

    if (sscanf(path, "/%127[^/]/%127s", user, file) == 2) {
        if (!find_user(user))
            return -ENOENT;

        st->st_mode = S_IFREG | 0444;
        st->st_nlink = 1;
        st->st_size = 128;
        return 0;
    }

    if (sscanf(path, "/%127s", user) == 1) {
        if (!find_user(user))
            return -ENOENT;

        st->st_mode = S_IFDIR | 0555;
        st->st_nlink = 2;
        return 0;
    }

    return -ENOENT;
}

int vfs_readdir(
    const char *path,
    void *buf,
    fuse_fill_dir_t filler,
    off_t off,
    struct fuse_file_info *fi,
    enum fuse_readdir_flags flags
) {
    (void) off; (void) fi; (void) flags;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    if (!strcmp(path, "/")) {
        for (int i = 0; i < users_count; i++)
            filler(buf, users[i].name, NULL, 0, 0);
        return 0;
    }

    char user[128];
    if (sscanf(path, "/%127s", user) == 1 && find_user(user)) {
        filler(buf, "id", NULL, 0, 0);
        filler(buf, "home", NULL, 0, 0);
        filler(buf, "shell", NULL, 0, 0);
        return 0;
    }

    return -ENOENT;
}

int vfs_open(const char *path, struct fuse_file_info *fi) {
    (void) fi;
    char user[128], file[128];
    return sscanf(path, "/%127[^/]/%127s", user, file) == 2 && find_user(user)
        ? 0 : -ENOENT;
}

int vfs_read(
    const char *path,
    char *buf,
    size_t size,
    off_t offset,
    struct fuse_file_info *fi
) {
    (void) fi;
    char user[128], file[128];

    if (sscanf(path, "/%127[^/]/%127s", user, file) != 2)
        return -ENOENT;

    user_info *u = find_user(user);
    if (!u)
        return -ENOENT;

    char data[256];
    if (!strcmp(file, "id")) snprintf(data, sizeof(data), "%d", u->uid);
    else if (!strcmp(file, "home")) snprintf(data, sizeof(data), "%s", u->home);
    else if (!strcmp(file, "shell")) snprintf(data, sizeof(data), "%s", u->shell);
    else return -ENOENT;

    size_t len = strlen(data);
    if (offset >= (off_t)len) return 0;
    if (offset + size > len) size = len - offset;

    memcpy(buf, data + offset, size);
    return size;
}

/* -------------------------------------------------- */

static struct fuse_operations ops = {
    .getattr = vfs_getattr,
    .readdir = vfs_readdir,
    .open    = vfs_open,
    .read    = vfs_read,
};

/* -------------------------------------------------- */
/* API для kubsh */

void init_users_dir(void) {
    mkdir(USERS_DIR, 0755);
    load_users();
    sync_users_from_dirs();
}

void rebuild_users_vfs(void) {
    load_users();
    sync_users_from_dirs();
}

void list_users_vfs(void) {
    for (int i = 0; i < users_count; i++)
        printf("%s\n", users[i].name);
}

int start_users_vfs(const char *mount_point) {
    (void) mount_point;

    init_users_dir();

    int pid = fork();
    if (pid == 0) {
        char *argv[] = { "kubsh-vfs", "-f", USERS_DIR, NULL };
        fuse_main(3, argv, &ops, NULL);
        _exit(0);
    }

    vfs_pid = pid;
    return 0;
}

void stop_users_vfs(void) {
    if (vfs_pid > 0) {
        kill(vfs_pid, SIGTERM);
        waitpid(vfs_pid, NULL, 0);
        vfs_pid = -1;
    }
}
