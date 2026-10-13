/* admin.h - 관리자(root) 권한 */
#ifndef TGUI_ADMIN_H
#define TGUI_ADMIN_H
#include "common.h"

BOOL is_admin(void);
/* Windows: runas 로 새 프로세스를 띄우고 TRUE. Linux: sudo 로 exec (성공하면 돌아오지 않음) */
BOOL relaunch_elevated(int argc, char **argv);
const char *admin_hint(void);          /* 권한 부족 시 안내 문구 */

#endif
