/* settings.h - 마지막으로 적용한 설정(열 포트 등)을 저장/불러오기 */
#ifndef TGUI_SETTINGS_H
#define TGUI_SETTINGS_H
#include "common.h"

/* role: "vps" 또는 "gamepc" */
BOOL settings_save(const char *role, const char *public_ip);
/* 저장된 설정을 O 에 기본값으로 적용 (파일이 없거나 값이 잘못되면 그 항목은 무시) */
void settings_load(void);
/* 저장된 역할을 읽는다. 없으면 FALSE */
BOOL settings_role(char *out, size_t sz);
void settings_path(char *out, size_t sz);

#endif
