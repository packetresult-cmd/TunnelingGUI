/* http.h - HTTP(S) GET (Windows: WinHTTP, Linux: curl) */
#ifndef TGUI_HTTP_H
#define TGUI_HTTP_H
#include "common.h"

/* 응답을 메모리(buf/cap) 또는 파일(fp)로 받는다 */
typedef struct { char *buf; size_t cap, len; FILE *fp; } Sink;

/* 플랫폼 구현 */
BOOL http_get(const char *url, const char *hdrs, Sink *s, DWORD tmo, BOOL progress);
/* 공통 */
BOOL http_text(const char *url, const char *hdrs, char *out, size_t sz, DWORD tmo);
BOOL egress_ip(char *out, size_t sz);

#endif
