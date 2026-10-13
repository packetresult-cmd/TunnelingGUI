/* fsutil.h - 파일/폴더 유틸 */
#ifndef TGUI_FSUTIL_H
#define TGUI_FSUTIL_H
#include "common.h"

/* 공통 (fsutil.c) */
char *read_file(const char *path);                 /* free() 필요. 실패 시 NULL */
void join(char *out, size_t sz, const char *a, const char *b);
BOOL ensure_workdir(void);

/* 플랫폼 구현 */
BOOL file_exists(const char *p);
BOOL write_file(const char *path, const char *text);   /* POSIX 는 0600 으로 생성 */
BOOL make_dir(const char *path);                       /* 이미 있으면 TRUE */
void rmtree(const char *dir);
void lock_dir(const char *dir);                        /* 관리자(root)만 쓰기. 비밀 파일은 각자 0600 */
void make_world_readable(const char *path);            /* 비밀이 아닌 파일(설정)을 GUI 등이 읽을 수 있게 (Windows: no-op) */

#endif
