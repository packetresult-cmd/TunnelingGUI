/* textutil.h - 입력 검증과 단순 텍스트 파싱 */
#ifndef TGUI_TEXTUTIL_H
#define TGUI_TEXTUTIL_H
#include "common.h"

void trim(char *s);                 /* 앞뒤 공백/개행 제거 */
BOOL valid_ip(const char *s);
BOOL valid_subnet(const char *s);
BOOL valid_dns(const char *s);
BOOL parse_ports(const char *s, int *arr, int *n);          /* "7777,27015" / "7777-7790" 범위 지원 */
void ports_str(const int *a, int n, char *out, size_t sz);  /* 연속 구간은 범위로 묶어 표시 */
void ports_plain(const int *a, int n, char *out, size_t sz); /* 하나씩 나열 */
BOOL json_val(const char *t, const char *section, const char *key, char *out, size_t sz);
BOOL conf_get(const char *text, const char *key, char *out, size_t sz);

#endif
