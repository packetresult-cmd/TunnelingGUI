/* fonts.h - 한글을 표시할 수 있는 시스템 글꼴 찾기 */
#ifndef TGUI_GUI_FONTS_H
#define TGUI_GUI_FONTS_H
#include <stdbool.h>
#include <stddef.h>

/* 환경 변수 TGUI_GUI_FONT → 플랫폼별 한글 글꼴 후보 순서로 찾는다. 찾으면 true */
bool find_korean_font(char *out, size_t sz);

#endif
