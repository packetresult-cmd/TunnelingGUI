/* ui.c
 * 플랫폼과 무관한 화면 보조 함수(경고, 확인 질문).
 */
#include "tgui.h"

void warn(const char *fmt, ...)
{
    char b[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    say(C_YELLOW, "경고: %s\n", b);
}

BOOL confirm(const char *prompt)
{
    char a[16];
    ask(prompt, NULL, a, sizeof a);
    return a[0] == 'y' || a[0] == 'Y';
}
