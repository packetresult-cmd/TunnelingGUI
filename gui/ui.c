/* ui.c
 * 즉시 모드 UI 구현. 글자는 SDL3_ttf 로 그리고, 같은 글자열은 텍스처를 캐시해 재사용한다.
 */
#include "ui.h"

const Col COL_BG     = { 0x1c, 0x1e, 0x24, 255 };
const Col COL_PANEL  = { 0x26, 0x29, 0x31, 255 };
const Col COL_PANEL2 = { 0x2f, 0x33, 0x3d, 255 };
const Col COL_BORDER = { 0x3c, 0x41, 0x4d, 255 };
const Col COL_TEXT   = { 0xe8, 0xea, 0xee, 255 };
const Col COL_DIM    = { 0x9a, 0xa1, 0xae, 255 };
const Col COL_ACCENT = { 0x3d, 0x7e, 0xf0, 255 };
const Col COL_GREEN  = { 0x4c, 0xc3, 0x7a, 255 };
const Col COL_RED    = { 0xe5, 0x4d, 0x5f, 255 };
const Col COL_YELLOW = { 0xf0, 0xc0, 0x4a, 255 };
const Col COL_INPUT  = { 0x18, 0x1a, 0x1f, 255 };

/* ------------------------------------------------------------------ 글자 텍스처 캐시 */
#define CACHE_N 1024
typedef struct {
    Uint32 hash;
    char *str;
    const TTF_Font *font;
    Uint32 color;
    SDL_Texture *tex;
    int w, h;
    Uint64 used;
} CacheEnt;
static CacheEnt g_cache[CACHE_N];

static Uint32 hash_str(const char *s, Uint32 seed)
{
    Uint32 h = 2166136261u ^ seed;
    for (; *s; s++) { h ^= (Uint8)*s; h *= 16777619u; }
    return h;
}

static CacheEnt *cached_text(UI *ui, TTF_Font *font, const char *s, Col c)
{
    Uint32 color = ((Uint32)c.r << 24) | ((Uint32)c.g << 16) | ((Uint32)c.b << 8) | c.a;
    Uint32 h = hash_str(s, color ^ (Uint32)(uintptr_t)font);
    int slot = (int)(h % CACHE_N), victim = -1;
    Uint64 oldest = (Uint64)-1;
    for (int k = 0; k < 16; k++) {
        CacheEnt *e = &g_cache[(slot + k) % CACHE_N];
        if (e->str && e->hash == h && e->font == font && e->color == color && !SDL_strcmp(e->str, s)) {
            e->used = ui->frame;
            return e;
        }
        if (!e->str) { victim = (slot + k) % CACHE_N; break; }
        if (e->used < oldest) { oldest = e->used; victim = (slot + k) % CACHE_N; }
    }
    {
        CacheEnt *e = &g_cache[victim];
        SDL_Surface *surf;
        if (e->tex) SDL_DestroyTexture(e->tex);
        SDL_free(e->str);
        SDL_zerop(e);
        surf = TTF_RenderText_Blended(font, s, 0, (SDL_Color){ c.r, c.g, c.b, c.a });
        if (!surf) return NULL;
        e->tex = SDL_CreateTextureFromSurface(ui->ren, surf);
        e->w = surf->w;
        e->h = surf->h;
        SDL_DestroySurface(surf);
        if (!e->tex) return NULL;
        e->str = SDL_strdup(s);
        e->hash = h;
        e->font = font;
        e->color = color;
        e->used = ui->frame;
        return e;
    }
}

/* ------------------------------------------------------------------ 기본 */
float S(const UI *ui, float v) { return v * ui->s; }

bool ui_init(UI *ui, SDL_Window *win, SDL_Renderer *ren, const char *font_path)
{
    float px;
    SDL_zerop(ui);
    ui->win = win;
    ui->ren = ren;
    ui->s = SDL_GetWindowDisplayScale(win);
    if (ui->s < 1.0f) ui->s = 1.0f;
    px = 15.0f * ui->s;
    ui->font = TTF_OpenFont(font_path, px);
    ui->font_bold = TTF_OpenFont(font_path, px * 1.15f);
    if (!ui->font || !ui->font_bold) return false;
    TTF_SetFontStyle(ui->font_bold, TTF_STYLE_BOLD);
    ui->line_h = TTF_GetFontHeight(ui->font);
    return true;
}

void ui_quit(UI *ui)
{
    for (int i = 0; i < CACHE_N; i++) {
        if (g_cache[i].tex) SDL_DestroyTexture(g_cache[i].tex);
        SDL_free(g_cache[i].str);
        SDL_zero(g_cache[i]);
    }
    if (ui->font) TTF_CloseFont(ui->font);
    if (ui->font_bold) TTF_CloseFont(ui->font_bold);
}

void ui_begin(UI *ui)
{
    ui->frame++;
    ui->mpressed = ui->mreleased = false;
    ui->wheel = 0;
    ui->typed[0] = 0;
    ui->backspaces = 0;
    ui->enter = ui->paste = ui->select_all = false;
    ui->focus_taken = false;
}

void ui_event(UI *ui, SDL_Event *e)
{
    SDL_ConvertEventToRenderCoordinates(ui->ren, e);
    switch (e->type) {
    case SDL_EVENT_MOUSE_MOTION: ui->mx = e->motion.x; ui->my = e->motion.y; break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (e->button.button == SDL_BUTTON_LEFT) { ui->mdown = true; ui->mpressed = true; ui->mx = e->button.x; ui->my = e->button.y; }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e->button.button == SDL_BUTTON_LEFT) { ui->mdown = false; ui->mreleased = true; ui->mx = e->button.x; ui->my = e->button.y; }
        break;
    case SDL_EVENT_MOUSE_WHEEL: ui->wheel += e->wheel.y; break;
    case SDL_EVENT_TEXT_INPUT:
        SDL_strlcat(ui->typed, e->text.text, sizeof ui->typed);
        ui->compose[0] = 0;
        break;
    case SDL_EVENT_TEXT_EDITING:
        SDL_strlcpy(ui->compose, e->edit.text ? e->edit.text : "", sizeof ui->compose);
        break;
    case SDL_EVENT_KEY_DOWN:
        if (e->key.key == SDLK_BACKSPACE) ui->backspaces++;
        else if (e->key.key == SDLK_RETURN || e->key.key == SDLK_KP_ENTER) ui->enter = true;
        else if ((e->key.mod & SDL_KMOD_CTRL) && e->key.key == SDLK_V) ui->paste = true;
        else if ((e->key.mod & SDL_KMOD_CTRL) && e->key.key == SDLK_A) ui->select_all = true;
        break;
    default: break;
    }
}

void ui_end(UI *ui)
{
    /* 아무 칸도 클릭하지 않고 바탕을 눌렀으면 입력 끝 */
    if (ui->mpressed && !ui->focus_taken && ui->focus) {
        ui->focus = NULL;
        SDL_StopTextInput(ui->win);
    }
    if (ui->mreleased) ui->active = NULL;
}

bool ui_hit(const UI *ui, SDL_FRect r)
{
    return ui->mx >= r.x && ui->mx < r.x + r.w && ui->my >= r.y && ui->my < r.y + r.h;
}

void ui_fill(UI *ui, SDL_FRect r, Col c)
{
    SDL_SetRenderDrawColor(ui->ren, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(ui->ren, &r);
}

void ui_border(UI *ui, SDL_FRect r, Col c)
{
    SDL_SetRenderDrawColor(ui->ren, c.r, c.g, c.b, c.a);
    SDL_RenderRect(ui->ren, &r);
}

static float draw_text(UI *ui, TTF_Font *f, float x, float y, const char *s, Col c)
{
    CacheEnt *e;
    SDL_FRect d;
    if (!s || !*s) return 0;
    e = cached_text(ui, f, s, c);
    if (!e) return 0;
    d = (SDL_FRect){ SDL_floorf(x), SDL_floorf(y), (float)e->w, (float)e->h };
    SDL_RenderTexture(ui->ren, e->tex, NULL, &d);
    return (float)e->w;
}

float ui_text(UI *ui, float x, float y, const char *s, Col c) { return draw_text(ui, ui->font, x, y, s, c); }
float ui_text_bold(UI *ui, float x, float y, const char *s, Col c) { return draw_text(ui, ui->font_bold, x, y, s, c); }

void ui_text_size(UI *ui, const char *s, int *w, int *h)
{
    if (!s || !*s) { *w = 0; *h = ui->line_h; return; }
    if (!TTF_GetStringSize(ui->font, s, 0, w, h)) { *w = 0; *h = ui->line_h; }
}

size_t ui_fit(UI *ui, const char *s, size_t len, int max_w)
{
    int mw = 0;
    size_t ml = 0;
    if (!TTF_MeasureString(ui->font, s, len, max_w, &mw, &ml)) return len;
    return ml;
}

/* ------------------------------------------------------------------ 위젯 */
static bool clicked(UI *ui, const void *id, SDL_FRect r, bool enabled)
{
    bool hit = ui_hit(ui, r);
    if (!enabled) return false;
    if (hit && ui->mpressed) ui->active = id;
    return hit && ui->mreleased && ui->active == id;
}

bool ui_button(UI *ui, const void *id, SDL_FRect r, const char *label, bool enabled, ButtonStyle st)
{
    bool hit = enabled && ui_hit(ui, r), res = clicked(ui, id, r, enabled);
    Col bg = st == BTN_PRIMARY ? COL_ACCENT : st == BTN_DANGER ? (Col){ 0x6b, 0x2a, 0x33, 255 } : COL_PANEL2;
    Col fg = enabled ? COL_TEXT : COL_DIM;
    int w, h;
    if (!enabled) bg = (Col){ 0x2a, 0x2d, 0x34, 255 };
    else if (hit) { bg.r = (Uint8)SDL_min(255, bg.r + 18); bg.g = (Uint8)SDL_min(255, bg.g + 18); bg.b = (Uint8)SDL_min(255, bg.b + 18); }
    if (enabled && ui->active == id && ui->mdown) { bg.r = (Uint8)(bg.r * 0.85f); bg.g = (Uint8)(bg.g * 0.85f); bg.b = (Uint8)(bg.b * 0.85f); }
    ui_fill(ui, r, bg);
    ui_border(ui, r, st == BTN_DANGER && enabled ? COL_RED : COL_BORDER);
    ui_text_size(ui, label, &w, &h);
    ui_text(ui, r.x + (r.w - (float)w) / 2, r.y + (r.h - (float)h) / 2, label, fg);
    return res;
}

bool ui_checkbox(UI *ui, const void *id, float x, float y, const char *label, bool *v, bool enabled)
{
    float box = S(ui, 18);
    int w, h;
    SDL_FRect b, area;
    ui_text_size(ui, label, &w, &h);
    b = (SDL_FRect){ x, y + ((float)ui->line_h - box) / 2, box, box };
    area = (SDL_FRect){ x, y, box + S(ui, 8) + (float)w, (float)ui->line_h };
    ui_fill(ui, b, COL_INPUT);
    ui_border(ui, b, enabled && ui_hit(ui, area) ? COL_ACCENT : COL_BORDER);
    if (*v) {
        SDL_FRect in = { b.x + S(ui, 4), b.y + S(ui, 4), b.w - S(ui, 8), b.h - S(ui, 8) };
        ui_fill(ui, in, enabled ? COL_ACCENT : COL_DIM);
    }
    ui_text(ui, x + box + S(ui, 8), y, label, enabled ? COL_TEXT : COL_DIM);
    if (clicked(ui, id, area, enabled)) { *v = !*v; return true; }
    return false;
}

static void delete_last_utf8(char *buf)
{
    size_t n = SDL_strlen(buf);
    while (n > 0) {
        n--;
        if (((unsigned char)buf[n] & 0xC0) != 0x80) break;
    }
    buf[n] = 0;
}

bool ui_textfield(UI *ui, const void *id, SDL_FRect r, char *buf, size_t cap, const char *placeholder, bool enabled)
{
    bool changed = false, focused;
    float pad = S(ui, 8), tx;
    int w, h;

    if (enabled && ui_hit(ui, r) && ui->mpressed) {
        if (ui->focus != id) { ui->focus = id; SDL_StartTextInput(ui->win); }
        ui->focus_taken = true;
    } else if (ui->mpressed && ui_hit(ui, r)) {
        ui->focus_taken = true;
    }
    focused = enabled && ui->focus == id;

    if (focused) {
        if (ui->select_all) { buf[0] = 0; changed = true; }
        for (int i = 0; i < ui->backspaces && buf[0]; i++) { delete_last_utf8(buf); changed = true; }
        if (ui->typed[0]) {
            for (const char *p = ui->typed; *p; p++)
                if ((unsigned char)*p >= 0x20 && SDL_strlen(buf) + 1 < cap) { size_t n = SDL_strlen(buf); buf[n] = *p; buf[n + 1] = 0; changed = true; }
        }
        if (ui->paste && SDL_HasClipboardText()) {
            char *clip = SDL_GetClipboardText();
            if (clip) {
                for (char *p = clip; *p; p++) if (*p == '\r' || *p == '\n' || *p == '\t') *p = ' ';
                SDL_strlcat(buf, clip, cap);
                SDL_free(clip);
                changed = true;
            }
        }
        if (ui->enter) { ui->focus = NULL; SDL_StopTextInput(ui->win); }
        ui->focus_rect = r;
        {
            SDL_Rect ir = { (int)r.x, (int)r.y, (int)r.w, (int)r.h };
            SDL_SetTextInputArea(ui->win, &ir, 0);
        }
    }

    ui_fill(ui, r, enabled ? COL_INPUT : COL_PANEL);
    ui_border(ui, r, focused ? COL_ACCENT : COL_BORDER);
    SDL_SetRenderClipRect(ui->ren, &(SDL_Rect){ (int)r.x + 1, (int)r.y + 1, (int)r.w - 2, (int)r.h - 2 });
    ui_text_size(ui, buf, &w, &h);
    tx = r.x + pad;
    if ((float)w > r.w - 2 * pad - S(ui, 40)) tx = r.x + r.w - pad - S(ui, 40) - (float)w;   /* 길면 끝이 보이게 */
    if (buf[0]) ui_text(ui, tx, r.y + (r.h - (float)ui->line_h) / 2, buf, enabled ? COL_TEXT : COL_DIM);
    else if (!focused && placeholder) ui_text(ui, r.x + pad, r.y + (r.h - (float)ui->line_h) / 2, placeholder, COL_DIM);
    if (focused) {
        float cx = tx + (float)w;
        if (ui->compose[0]) {
            int cw, ch;
            ui_text_size(ui, ui->compose, &cw, &ch);
            ui_text(ui, cx, r.y + (r.h - (float)ui->line_h) / 2, ui->compose, COL_YELLOW);
            ui_fill(ui, (SDL_FRect){ cx, r.y + r.h - S(ui, 6), (float)cw, S(ui, 1) }, COL_YELLOW);
            cx += (float)cw;
        }
        if ((SDL_GetTicks() / 500) % 2 == 0) ui_fill(ui, (SDL_FRect){ cx + 1, r.y + S(ui, 6), S(ui, 2), r.h - S(ui, 12) }, COL_TEXT);
    }
    SDL_SetRenderClipRect(ui->ren, NULL);
    return changed;
}

bool ui_tab(UI *ui, const void *id, SDL_FRect r, const char *label, bool selected)
{
    bool hit = ui_hit(ui, r), res = clicked(ui, id, r, true);
    int w, h;
    ui_fill(ui, r, selected ? COL_PANEL : (hit ? COL_PANEL2 : COL_BG));
    if (selected) ui_fill(ui, (SDL_FRect){ r.x, r.y, r.w, S(ui, 3) }, COL_ACCENT);
    ui_text_size(ui, label, &w, &h);
    ui_text(ui, r.x + (r.w - (float)w) / 2, r.y + (r.h - (float)h) / 2, label, selected ? COL_TEXT : COL_DIM);
    return res;
}
