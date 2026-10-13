/* ports.h - 포트 목록 문자열 처리 (CLI 의 textutil 과 같은 규칙: "7777,27015", 범위 "7777-7790", 최대 256개) */
#ifndef TGUI_GUI_PORTS_H
#define TGUI_GUI_PORTS_H
#include <stdbool.h>
#include <stddef.h>

#define GUI_MAX_PORTS 256

typedef struct { int v[GUI_MAX_PORTS]; int n; } PortSet;

/* 목록을 집합에 더한다 (중복 제거, 정렬). 빈 문자열은 성공(0개). 형식 오류/개수 초과면 false 와 이유 */
bool ports_add_list(PortSet *ps, const char *s, char *why, size_t wsz);
bool ports_add(PortSet *ps, int port);
bool ports_has(const PortSet *ps, int port);
/* 연속 3개 이상은 범위로 묶어 문자열로 ("7777-7790,27015"), 비면 "" */
void ports_format(const PortSet *ps, char *out, size_t sz);

#endif
