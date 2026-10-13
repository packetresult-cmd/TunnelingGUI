/* win/fs.c
 * Windows 파일 시스템 구현: 존재 확인, 쓰기, 폴더 생성/삭제, 접근 제한(ACL).
 */
#include "tgui.h"

BOOL file_exists(const char *p) { return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES; }

BOOL write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    size_t n = strlen(text);
    BOOL ok;
    if (!f) return FALSE;
    ok = fwrite(text, 1, n, f) == n;
    return fclose(f) == 0 && ok;
}

BOOL make_dir(const char *path)
{
    return CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

/* 설정 폴더(TunnelingGUI)만 지우는 용도. 심볼릭 링크/정션은 따라가지 않는다 */
void rmtree(const char *dir)
{
    char pat[MAX_PATH], p[MAX_PATH];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    snprintf(pat, sizeof pat, "%s\\*", dir);
    h = FindFirstFileA(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
            join(p, sizeof p, dir, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) RemoveDirectoryA(p); else DeleteFileA(p);
            } else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                rmtree(p);
            } else {
                SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL);
                DeleteFileA(p);
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryA(dir);
}

/* 관리자/SYSTEM 만 접근 (SID 사용: 현지화된 그룹 이름 문제 회피) */
void lock_dir(const char *dir)
{
    char cmd[MAX_PATH + 160];
    snprintf(cmd, sizeof cmd, "icacls \"%s\" /inheritance:r /grant:r *S-1-5-32-544:(OI)(CI)F *S-1-5-18:(OI)(CI)F", dir);
    run_ok(cmd);
}

/* Windows: 작업 폴더 ACL(관리자/SYSTEM)을 그대로 둔다. GUI 도 관리자 권한으로 실행된다 */
void make_world_readable(const char *path)
{
    (void)path;
}
