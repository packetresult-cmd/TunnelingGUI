/* pkg.h - (Linux) 배포판 패키지 관리자 보조 */
#ifndef TGUI_PKG_H
#define TGUI_PKG_H
#include "common.h"

BOOL have_cmd(const char *cmd);        /* PATH 에 명령이 있는가 */
BOOL ensure_cmd(const char *cmd);      /* 없으면 알려진 패키지로 자동 설치. 성공(이미 있음 포함)=TRUE */

#endif
