/* cli.h - 명령행과 메뉴 */
#ifndef TGUI_CLI_H
#define TGUI_CLI_H
#include "common.h"

void usage(void);
BOOL flag(const char *a, const char *name);
BOOL parse_opts(int argc, char **argv, int from);
BOOL validate_opts(void);
int  dispatch(const char *cmd);
int  menu(void);

#endif
