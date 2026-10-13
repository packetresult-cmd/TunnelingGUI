/* ui.h - SDL3 렌더러 위에 그리는 작은 즉시 모드(immediate-mode) UI
 * 매 프레임 위젯 함수를 호출하면 그리고, 클릭/입력 결과를 바로 돌려준다. */
#ifndef TGUI_GUI_UI_H
#define TGUI_GUI_UI_H
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

typedef struct { Uint8 r, g, b, a; } Col;

extern const Col COL_BG, COL_PANEL, COL_PANEL2, COL_BORDER, COL_TEXT, COL_DIM, COL_ACCENT,
                 COL_GREEN, COL_RED, COL_YELLOW, COL_INPUT;

typedef enum { BTN_NORMAL, BTN_PRIMARY, BTN_DANGER } ButtonStyle;

typedef struct UI {
    SDL_Window   *win;
    SDL_Renderer *ren;
    TTF_Font     *font, *font_bold;
    float         s;              /* 화면 배율 (고해상도 모니터) */
    int           line_h;         /* 글자 한 줄 높이(px) */
    Uint64        frame;

    /* 마우스 (렌더 좌표) */
    float mx, my, wheel;
    bool  mdown, mpressed, mreleased;
    const void *active;           /* 누르고 있는 위젯 */

    /* 키보드 / 글자 입력 */
    const void *focus;            /* 입력 중인 칸 */
    bool  focus_taken;            /* 이번 프레임 클릭을 입력 칸이 가져갔는가 */
    char  typed[512];             /* 이번 프레임에 들어온 글자 (UTF-8) */
    int   backspaces;
    bool  enter, paste, select_all;
    char  compose[128];           /* IME 조합 중인 글자 (한글 등) */
    SDL_FRect focus_rect;
} UI;

bool  ui_init(UI *ui, SDL_Window *win, SDL_Renderer *ren, const char *font_path);
void  ui_quit(UI *ui);
void  ui_begin(UI *ui);                       /* 프레임 시작 (입력 상태 초기화 전) */
void  ui_event(UI *ui, SDL_Event *e);         /* 이벤트 전달 */
void  ui_end(UI *ui);                         /* 프레임 끝 */
float S(const UI *ui, float v);               /* 배율 적용 */

void  ui_fill(UI *ui, SDL_FRect r, Col c);
void  ui_border(UI *ui, SDL_FRect r, Col c);
float ui_text(UI *ui, float x, float y, const char *s, Col c);          /* 반환: 너비 */
float ui_text_bold(UI *ui, float x, float y, const char *s, Col c);
void  ui_text_size(UI *ui, const char *s, int *w, int *h);
/* max_w 안에 들어가는 바이트 수 (줄바꿈용) */
size_t ui_fit(UI *ui, const char *s, size_t len, int max_w);

bool  ui_button(UI *ui, const void *id, SDL_FRect r, const char *label, bool enabled, ButtonStyle st);
bool  ui_checkbox(UI *ui, const void *id, float x, float y, const char *label, bool *v, bool enabled);
bool  ui_textfield(UI *ui, const void *id, SDL_FRect r, char *buf, size_t cap, const char *placeholder, bool enabled);
bool  ui_tab(UI *ui, const void *id, SDL_FRect r, const char *label, bool selected);
bool  ui_hit(const UI *ui, SDL_FRect r);

#endif
