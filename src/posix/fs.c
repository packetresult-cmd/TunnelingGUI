/* posix/fs.c
 * Linux 파일 시스템 구현. 개인키가 들어갈 수 있으므로 파일은 0600, 폴더는 0700 으로 만든다.
 */
#include "tgui.h"
#include <fcntl.h>
#include <dirent.h>

BOOL file_exists(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0;
}

BOOL write_file(const char *path, const char *text)
{
    size_t n = strlen(text), done = 0;
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return FALSE;
    fchmod(fd, 0600);
    while (done < n) {
        ssize_t w = write(fd, text + done, n - done);
        if (w <= 0) { close(fd); return FALSE; }
        done += (size_t)w;
    }
    return close(fd) == 0;
}

BOOL make_dir(const char *path)
{
    return mkdir(path, 0700) == 0 || errno == EEXIST;
}

/* 설정 폴더(/etc/tgui)만 지우는 용도. 심볼릭 링크는 따라가지 않는다 */
void rmtree(const char *dir)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    char p[MAX_PATH * 2];
    struct stat st;
    if (d) {
        while ((e = readdir(d)) != NULL) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
            if (lstat(p, &st) != 0) continue;
            if (S_ISDIR(st.st_mode)) rmtree(p); else unlink(p);
        }
        closedir(d);
    }
    rmdir(dir);
}

/* 폴더는 root 만 쓰기(0755). 개인키가 든 파일은 write_file() 이 0600 으로 만든다.
 * settings.conf 처럼 비밀이 아닌 파일은 GUI 가 root 권한 없이 읽을 수 있게 한다 */
void lock_dir(const char *dir)
{
    chmod(dir, 0755);
}

void make_world_readable(const char *path)
{
    chmod(path, 0644);
}
