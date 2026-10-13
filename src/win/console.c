/* win/console.c
 * Windows UTF-8 콘솔 입출력(색상, 질문, 종료 시 일시정지).
 */
#include "tgui.h"

static HANDLE g_out, g_in;
static BOOL g_con, g_con_in;

void console_init(void)
{
    DWORD mode;
    SetConsoleOutputCP(CP_UTF8);
    g_out = GetStdHandle(STD_OUTPUT_HANDLE);
    g_in = GetStdHandle(STD_INPUT_HANDLE);
    g_con = GetConsoleMode(g_out, &mode);
    g_con_in = GetConsoleMode(g_in, &mode);
}

void put(const char *s)
{
    DWORD n;
    if (g_con) {
        wchar_t w[8192];
        int l = MultiByteToWideChar(CP_UTF8, 0, s, -1, w, 8192);
        if (l > 1) WriteConsoleW(g_out, w, (DWORD)(l - 1), &n, NULL);
    } else {
        WriteFile(g_out, s, (DWORD)strlen(s), &n, NULL);
    }
}

void say(int color, const char *fmt, ...)
{
    char b[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (g_con) SetConsoleTextAttribute(g_out, (WORD)color);
    put(b);
    if (g_con) SetConsoleTextAttribute(g_out, C_DEF);
}

void ask(const char *prompt, const char *def, char *out, size_t sz)
{
    char line[512] = "";
    if (def && *def) say(C_DEF, "%s [%s]: ", prompt, def); else say(C_DEF, "%s: ", prompt);
    if (g_con_in) {
        wchar_t w[512];
        DWORD n = 0;
        if (ReadConsoleW(g_in, w, 511, &n, NULL) && n) {
            w[n] = 0;
            WideCharToMultiByte(CP_UTF8, 0, w, -1, line, sizeof line, NULL, NULL);
        }
    } else if (!fgets(line, sizeof line, stdin)) {
        line[0] = 0;
    }
    trim(line);
    snprintf(out, sz, "%s", (*line || !def) ? line : def);
}


void pause_if_own_console(void)
{
    DWORD pids[2];
    if (g_con && GetConsoleProcessList(pids, 2) <= 1) {
        pf("\n계속하려면 Enter 키를 누르세요...");
        char tmp[8];
        ask("", NULL, tmp, sizeof tmp);
    }
}
