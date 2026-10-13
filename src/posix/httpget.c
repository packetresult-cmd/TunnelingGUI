/* posix/httpget.c
 * Linux HTTP GET: 시스템에 있는 curl 을 사용한다 (별도 라이브러리 링크 없음).
 * 응답은 메모리 버퍼로만 받는다 (파일 다운로드는 패키지 관리자가 담당).
 */
#include "tgui.h"
#include "pkg.h"

BOOL http_get(const char *url, const char *hdrs, Sink *s, DWORD tmo, BOOL progress)
{
    char cmd[2048];
    DWORD code = 1;
    (void)progress;

    if (s->fp || !s->buf || !s->cap) return FALSE;
    if (strchr(url, '\'') || (hdrs && strchr(hdrs, '\''))) return FALSE;   /* sh 인용 깨짐 방지 */
    if (!have_cmd("curl")) return FALSE;

    snprintf(cmd, sizeof cmd, "curl -fsSL --max-time %lu%s%s%s '%s'",
             (unsigned long)(tmo / 1000 ? tmo / 1000 : 1),
             hdrs ? " -H '" : "", hdrs ? hdrs : "", hdrs ? "'" : "", url);
    if (!run(cmd, NULL, s->buf, s->cap, &code) || code != 0) { s->buf[0] = 0; return FALSE; }
    s->len = strlen(s->buf);
    return TRUE;
}
