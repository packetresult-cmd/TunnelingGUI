/* console.h - 콘솔 출력/입력 (Windows: UTF-8 콘솔 API, POSIX: ANSI 색상) */
#ifndef TGUI_CONSOLE_H
#define TGUI_CONSOLE_H
#include "common.h"

enum { C_DEF = 7, C_CYAN = 11, C_GREEN = 10, C_YELLOW = 14, C_RED = 12 };

/* 플랫폼 구현 */
void console_init(void);
void put(const char *s);
void say(int color, const char *fmt, ...);
void ask(const char *prompt, const char *def, char *out, size_t sz);
void pause_if_own_console(void);   /* 더블클릭으로 열린 창이 바로 닫히지 않게 (Windows 전용, POSIX 는 no-op) */

/* 공통 (ui.c) */
void warn(const char *fmt, ...);
BOOL confirm(const char *prompt);

#define pf(...)    say(C_DEF, __VA_ARGS__)
#define step(...)  do { say(C_CYAN, "==> " __VA_ARGS__); pf("\n"); } while (0)

#endif
