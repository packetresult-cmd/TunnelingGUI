/* posix/console.c
 * Linux 콘솔 입출력: ANSI 색상, 질문.
 */
#include "tgui.h"
#include <signal.h>

static int g_tty;

void console_init(void)
{
    g_tty = isatty(STDOUT_FILENO);
    signal(SIGPIPE, SIG_IGN);   /* 자식 프로세스 stdin 으로 쓰다 끊겨도 죽지 않게 */
}

void put(const char *s)
{
    fputs(s, stdout);
    fflush(stdout);
}

void say(int color, const char *fmt, ...)
{
    char b[4096];
    const char *on = "";
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (g_tty) {
        switch (color) {
        case C_CYAN:   on = "\x1b[36m"; break;
        case C_GREEN:  on = "\x1b[32m"; break;
        case C_YELLOW: on = "\x1b[33m"; break;
        case C_RED:    on = "\x1b[31m"; break;
        default: break;
        }
    }
    if (*on) fputs(on, stdout);
    put(b);
    if (*on) put("\x1b[0m");
}

void ask(const char *prompt, const char *def, char *out, size_t sz)
{
    char line[512] = "";
    if (def && *def) say(C_DEF, "%s [%s]: ", prompt, def); else say(C_DEF, "%s: ", prompt);
    if (!fgets(line, sizeof line, stdin)) line[0] = 0;
    trim(line);
    snprintf(out, sz, "%s", (*line || !def) ? line : def);
}

void pause_if_own_console(void) { }
