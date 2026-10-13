/* main.c
 * TunnelingGUI GUI (SDL3 + SDL3_ttf).
 *
 * GUI 는 화면만 담당하고, 실제 작업은 같은 폴더의 tgui CLI 를 실행해서 한다.
 * (CLI 에서 검증된 동작을 그대로 쓰고, 출력은 아래 "실행 기록" 에 실시간으로 보여준다)
 *   Windows: GUI 가 관리자 권한(매니페스트)으로 실행되고, CLI 는 그 권한을 물려받는다.
 *   Linux  : root 가 아니면 동작마다 pkexec 로 관리자 인증을 받는다.
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include "fonts.h"
#include "i18n.h"
#include "ports.h"
#include "runner.h"
#include "ui.h"

#ifndef TGUI_VERSION                       /* CMake passes the project version in */
#  define TGUI_VERSION "0.1.0"
#endif
#define APP_TITLE     "TunnelingGUI"
#define LOG_MAX_LINES 3000

typedef struct { char *text; Col color; int rows; int rows_w; } LogLine;

typedef struct {
    UI      ui;
    Runner  run;
    int     tab;                      /* 0 VPS, 1 게임 PC, 2 검증 */

    /* 열 포트 */
    bool    p7777, p7778, p27015, p27020;
    char    extra_udp[1024], extra_tcp[1024];
    bool    have_saved;
    char    saved_role[16], saved_udp[2048], saved_tcp[2048];

    /* VPS */
    char    public_ip[64], wg_port[8];
    bool    purge, disable_routing;
    /* 게임 PC */
    char    conf_path[1024];
    bool    keep_config;
    /* 검증 */
    char    target[128], qport[8], steam_key[128];

    /* 실행 기록 */
    LogLine *lines;
    int      nlines;
    char     partial[8192];
    float    scroll;                  /* 위에서부터 픽셀 */
    bool     follow;
    char     status[256];
    Col      status_col;

    /* 환경 */
    char    cli[1024];
    bool    cli_ok, elevated, use_pkexec;
    char    settings_file[1024];

    /* language */
    int     lang_idx;
    char    lang_file[1024];

    /* 파일 선택 대화상자 결과 (다른 스레드에서 올 수 있다) */
    SDL_Mutex *dlg_lock;
    char    dlg_path[1024];
    bool    dlg_ready;
} App;

/* ------------------------------------------------------------------ 실행 기록 */
static Col line_color(const char *s)
{
    if (!SDL_strncmp(s, "==> ", 4)) return COL_ACCENT;
    if (!SDL_strncmp(s, "$ ", 2)) return COL_DIM;
    /* The CLI's own language is independent of the GUI's, so match both. */
    if (SDL_strstr(s, "경고") == s || SDL_strstr(s, "Warning") == s) return COL_YELLOW;
    if (SDL_strstr(s, "실패") || SDL_strstr(s, "[실행 실패]") || SDL_strstr(s, "오류") == s ||
        SDL_strstr(s, "잘못된") == s || SDL_strstr(s, "알 수 없는") == s ||
        SDL_strstr(s, "Error") == s || SDL_strstr(s, "failed") || SDL_strstr(s, "FAILED")) return COL_RED;
    if (SDL_strstr(s, "완료") == s || SDL_strstr(s, "성공") == s || SDL_strstr(s, "점검 통과") == s ||
        SDL_strstr(s, "OK:") || SDL_strstr(s, "Done") == s || SDL_strstr(s, "Success") == s) return COL_GREEN;
    return COL_TEXT;
}

/* 일부 한글 글꼴(나눔 등)에 없는 글자를 바꿔서 넣는다 */
static void fix_glyphs(char *s)
{
    char *p;
    while ((p = SDL_strstr(s, "\xE2\x86\x92")) != NULL) {   /* → */
        p[0] = '-'; p[1] = '>';
        SDL_memmove(p + 2, p + 3, SDL_strlen(p + 3) + 1);
    }
}

static void log_line(App *a, const char *s, Col c)
{
    if (a->nlines >= LOG_MAX_LINES) {
        SDL_free(a->lines[0].text);
        SDL_memmove(a->lines, a->lines + 1, sizeof(LogLine) * (size_t)(a->nlines - 1));
        a->nlines--;
    } else {
        LogLine *n = (LogLine *)SDL_realloc(a->lines, sizeof(LogLine) * (size_t)(a->nlines + 1));
        if (!n) return;
        a->lines = n;
    }
    a->lines[a->nlines].text = SDL_strdup(s);
    if (a->lines[a->nlines].text) fix_glyphs(a->lines[a->nlines].text);
    a->lines[a->nlines].color = c;
    a->lines[a->nlines].rows = 1;
    a->lines[a->nlines].rows_w = -1;
    a->nlines++;
}

/* 스트림 출력: 줄 단위로 나눠 넣고, 끝나지 않은 줄은 partial 에 남긴다 */
static void log_stream(App *a, const char *data, bool flush)
{
    for (const char *p = data; p && *p; p++) {
        size_t n = SDL_strlen(a->partial);
        if (*p == '\r') continue;
        if (*p == '\n') { log_line(a, a->partial, line_color(a->partial)); a->partial[0] = 0; continue; }
        if (n + 1 < sizeof a->partial) { a->partial[n] = *p; a->partial[n + 1] = 0; }
    }
    if (flush && a->partial[0]) { log_line(a, a->partial, line_color(a->partial)); a->partial[0] = 0; }
    a->follow = true;
}

static void log_clear(App *a)
{
    for (int i = 0; i < a->nlines; i++) SDL_free(a->lines[i].text);
    SDL_free(a->lines);
    a->lines = NULL;
    a->nlines = 0;
    a->scroll = 0;
}

static void log_copy(App *a)
{
    size_t total = 1;
    char *all;
    for (int i = 0; i < a->nlines; i++) total += SDL_strlen(a->lines[i].text) + 2;
    all = (char *)SDL_malloc(total);
    if (!all) return;
    all[0] = 0;
    for (int i = 0; i < a->nlines; i++) { SDL_strlcat(all, a->lines[i].text, total); SDL_strlcat(all, "\n", total); }
    SDL_SetClipboardText(all);
    SDL_free(all);
}

/* ------------------------------------------------------------------ 저장된 설정 (settings.conf) */
static bool conf_value(const char *t, const char *key, char *out, size_t sz)
{
    size_t kl = SDL_strlen(key);
    for (const char *p = t; p && *p;) {
        if (!SDL_strncmp(p, key, kl) && p[kl] == '=') {
            size_t n = 0;
            while (p[kl + 1 + n] && p[kl + 1 + n] != '\r' && p[kl + 1 + n] != '\n') n++;
            if (n >= sz) n = sz - 1;
            SDL_memcpy(out, p + kl + 1, n);
            out[n] = 0;
            return true;
        }
        p = SDL_strchr(p, '\n');
        if (p) p++;
    }
    return false;
}

/* 저장된 포트를 체크박스 + 추가 칸으로 나눠 채운다 */
static void apply_saved_ports(App *a, const char *udp, const char *tcp)
{
    PortSet u = { 0 }, t = { 0 }, rest = { 0 };
    char why[128];
    if (!ports_add_list(&u, udp, why, sizeof why) || !ports_add_list(&t, tcp, why, sizeof why)) return;
    a->p7777 = ports_has(&u, 7777);
    a->p7778 = ports_has(&u, 7778);
    a->p27015 = ports_has(&u, 27015);
    for (int i = 0; i < u.n; i++) if (u.v[i] != 7777 && u.v[i] != 7778 && u.v[i] != 27015) ports_add(&rest, u.v[i]);
    ports_format(&rest, a->extra_udp, sizeof a->extra_udp);
    rest.n = 0;
    a->p27020 = ports_has(&t, 27020);
    for (int i = 0; i < t.n; i++) if (t.v[i] != 27020) ports_add(&rest, t.v[i]);
    ports_format(&rest, a->extra_tcp, sizeof a->extra_tcp);
}

static void load_settings(App *a, bool fill_fields)
{
    size_t len = 0;
    char *t = (char *)SDL_LoadFile(a->settings_file, &len);
    char v[32];
    a->have_saved = false;
    if (!t) return;
    a->have_saved = conf_value(t, "role", a->saved_role, sizeof a->saved_role);
    if (!conf_value(t, "udp", a->saved_udp, sizeof a->saved_udp)) a->saved_udp[0] = 0;
    if (!conf_value(t, "tcp", a->saved_tcp, sizeof a->saved_tcp)) a->saved_tcp[0] = 0;
    if (fill_fields) {
        apply_saved_ports(a, a->saved_udp, a->saved_tcp);
        if (conf_value(t, "wg_port", v, sizeof v) && *v) SDL_strlcpy(a->wg_port, v, sizeof a->wg_port);
    }
    SDL_free(t);
}

/* 체크박스 + 추가 칸 → 최종 포트 목록. 오류가 있으면 false */
static bool build_ports(App *a, char *udp, size_t usz, char *tcp, size_t tsz, int *nu, int *nt, char *why, size_t wsz)
{
    PortSet u = { 0 }, t = { 0 };
    char w2[128];
    why[0] = 0;
    if (a->p7777) ports_add(&u, 7777);
    if (a->p7778) ports_add(&u, 7778);
    if (a->p27015) ports_add(&u, 27015);
    if (a->p27020) ports_add(&t, 27020);
    if (!ports_add_list(&u, a->extra_udp, w2, sizeof w2)) { SDL_snprintf(why, wsz, T("ports.err.udp"), w2); return false; }
    if (!ports_add_list(&t, a->extra_tcp, w2, sizeof w2)) { SDL_snprintf(why, wsz, T("ports.err.tcp"), w2); return false; }
    ports_format(&u, udp, usz);
    ports_format(&t, tcp, tsz);
    *nu = u.n;
    *nt = t.n;
    return true;
}

/* ------------------------------------------------------------------ CLI 실행 */
static void run_cli(App *a, const char *title, const char *const *args)
{
    const char *argv[RUNNER_MAX_ARGS + 1];
    char shown[1024] = "$ tgui";
    int k = 0;

    if (runner_busy(&a->run)) return;
    if (!a->cli_ok) {
        log_line(a, T("cli.missing.err"), COL_RED);
        return;
    }
#ifndef _WIN32
    if (!a->elevated) {
        if (!a->use_pkexec) {
            log_line(a, T("cli.needroot"), COL_RED);
            return;
        }
        argv[k++] = "pkexec";
    }
#endif
    argv[k++] = a->cli;
    for (int i = 0; args[i] && k < RUNNER_MAX_ARGS; i++) {
        argv[k++] = args[i];
        SDL_strlcat(shown, " ", sizeof shown);
        /* 화면에는 API 키를 가린다 */
        SDL_strlcat(shown, (i > 0 && !SDL_strcmp(args[i - 1], "-steam-key")) ? "****" : args[i], sizeof shown);
    }
    argv[k] = NULL;
    log_line(a, "", COL_TEXT);
    log_line(a, shown, COL_DIM);
    a->follow = true;
    if (!runner_start(&a->run, argv, title)) {
        log_line(a, T("cli.threadfail"), COL_RED);
        return;
    }
    SDL_snprintf(a->status, sizeof a->status, T("cli.running"), title);
    a->status_col = COL_YELLOW;
}

static bool confirm(App *a, const char *title, const char *msg)
{
    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, T("dlg.cancel") },
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, T("dlg.continue") },
    };
    SDL_MessageBoxData d = { SDL_MESSAGEBOX_WARNING, a->ui.win, title, msg, 2, buttons, NULL };
    int id = 0;
    return SDL_ShowMessageBox(&d, &id) && id == 1;
}

static void SDLCALL on_file_chosen(void *ud, const char *const *files, int filter)
{
    App *a = (App *)ud;
    (void)filter;
    if (!files || !files[0]) return;
    SDL_LockMutex(a->dlg_lock);
    SDL_strlcpy(a->dlg_path, files[0], sizeof a->dlg_path);
    a->dlg_ready = true;
    SDL_UnlockMutex(a->dlg_lock);
}

/* 사용자 입력이 인자로 들어가기 전에 확인 (셸을 거치지 않지만 CLI 쪽 검증과 같은 범위로 미리 막는다) */
static bool safe_field(const char *s, const char *extra_ok)
{
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || SDL_strchr(extra_ok, c)) continue;
        return false;
    }
    return true;
}

/* ------------------------------------------------------------------ 화면 */

static void lang_next(App *a);   /* defined with the other language helpers below */
static float label(App *a, float x, float y, const char *s)
{
    ui_text(&a->ui, x, y, s, COL_DIM);
    return y + (float)a->ui.line_h + S(&a->ui, 4);
}

static float wrap_text(App *a, float x, float y, float w, const char *s, Col c)
{
    UI *ui = &a->ui;
    size_t len = SDL_strlen(s), pos = 0;
    char line[512];
    while (pos < len) {
        size_t n = ui_fit(ui, s + pos, len - pos, (int)w);
        if (n == 0) n = 1;
        /* Break at a space rather than mid-word. Scripts written without spaces
           (CJK) find none and keep the measured break, which is correct for them. */
        if (pos + n < len && s[pos + n] != ' ') {
            size_t b = n;
            while (b > 0 && s[pos + b - 1] != ' ') b--;
            if (b > 0) n = b;
        }
        if (n >= sizeof line) n = sizeof line - 1;
        SDL_memcpy(line, s + pos, n);
        line[n] = 0;
        ui_text(ui, x, y, line, c);
        y += (float)ui->line_h + S(ui, 2);
        pos += n;
    }
    return y;
}

static float draw_ports_panel(App *a, SDL_FRect r, bool busy)
{
    UI *ui = &a->ui;
    float x = r.x + S(ui, 14), y = r.y + S(ui, 12), w = r.w - S(ui, 28), fh = S(ui, 34);
    char udp[2048], tcp[2048], why[160], line[2600];
    int nu = 0, nt = 0;
    bool ok;

    ui_fill(ui, r, COL_PANEL);
    ui_text_bold(ui, x, y, T("ports.title"), COL_TEXT);
    y += (float)ui->line_h + S(ui, 12);
    ui_checkbox(ui, &a->p7777, x, y, T("ports.p7777"), &a->p7777, !busy);
    ui_checkbox(ui, &a->p7778, x + w / 2, y, T("ports.p7778"), &a->p7778, !busy);
    y += (float)ui->line_h + S(ui, 8);
    ui_checkbox(ui, &a->p27015, x, y, T("ports.p27015"), &a->p27015, !busy);
    ui_checkbox(ui, &a->p27020, x + w / 2, y, T("ports.p27020"), &a->p27020, !busy);
    y += (float)ui->line_h + S(ui, 14);

    y = label(a, x, y, T("ports.extraudp"));
    ui_textfield(ui, a->extra_udp, (SDL_FRect){ x, y, w, fh }, a->extra_udp, sizeof a->extra_udp, T("ports.extraudp.hint"), !busy);
    y += fh + S(ui, 10);
    y = label(a, x, y, T("ports.extratcp"));
    ui_textfield(ui, a->extra_tcp, (SDL_FRect){ x, y, w, fh }, a->extra_tcp, sizeof a->extra_tcp, T("ports.extratcp.hint"), !busy);
    y += fh + S(ui, 12);

    ok = build_ports(a, udp, sizeof udp, tcp, sizeof tcp, &nu, &nt, why, sizeof why);
    if (ok) {
        SDL_snprintf(line, sizeof line, T("ports.udpcount"), nu, nu ? udp : T("ports.none"));
        y = wrap_text(a, x, y, w, line, nu ? COL_TEXT : COL_YELLOW);
        SDL_snprintf(line, sizeof line, T("ports.tcpcount"), nt, nt ? tcp : T("ports.none"));
        y = wrap_text(a, x, y, w, line, COL_TEXT);
    } else {
        y = wrap_text(a, x, y, w, why, COL_RED);
    }
    y += S(ui, 4);
    if (a->have_saved) SDL_snprintf(line, sizeof line, T("ports.applied"), a->saved_role,
                                    *a->saved_udp ? a->saved_udp : T("ports.nonebare"), *a->saved_tcp ? a->saved_tcp : T("ports.nonebare"));
    else SDL_snprintf(line, sizeof line, "%s", T("ports.notapplied"));
    y = wrap_text(a, x, y, w, line, COL_DIM);
    y += S(ui, 8);

    if (ui_button(ui, "apply_ports", (SDL_FRect){ x, y, w, fh }, T("ports.apply"), ok && !busy && a->have_saved, BTN_NORMAL)) {
        const char *args[] = { "ports", "-udp", *udp ? udp : "-", "-tcp", *tcp ? tcp : "-", NULL };
        run_cli(a, T("ports.apply.task"), args);
    }
    y += fh + S(ui, 8);
    wrap_text(a, x, y, w, T("ports.note"), COL_DIM);
    return ok;
}

/* id1/id2 are stable widget identities (the UI keys off the pointer), kept separate
   from the labels so that switching language does not reshuffle widget state. */
static void two_buttons(App *a, float x, float y, float w, float h, const void *id1, const void *id2,
                        const char *l1, const char *l2, bool en, ButtonStyle s1, ButtonStyle s2, bool *c1, bool *c2)
{
    float gap = S(&a->ui, 10), bw = (w - gap) / 2;
    *c1 = ui_button(&a->ui, id1, (SDL_FRect){ x, y, bw, h }, l1, en, s1);
    *c2 = l2 ? ui_button(&a->ui, id2, (SDL_FRect){ x + bw + gap, y, bw, h }, l2, en, s2) : false;
}

static void draw_vps(App *a, SDL_FRect r, bool busy, bool ports_ok)
{
    UI *ui = &a->ui;
    float x = r.x + S(ui, 14), y = r.y + S(ui, 12), w = r.w - S(ui, 28), fh = S(ui, 34), bh = S(ui, 38);
    char udp[2048], tcp[2048], why[160];
    int nu, nt;
    bool c1, c2;

    ui_fill(ui, r, COL_PANEL);
    ui_text_bold(ui, x, y, T("vps.title"), COL_TEXT);
    y += (float)ui->line_h + S(ui, 12);
    y = label(a, x, y, T("vps.publicip"));
    ui_textfield(ui, a->public_ip, (SDL_FRect){ x, y, w * 0.62f, fh }, a->public_ip, sizeof a->public_ip, T("vps.publicip.hint"), !busy);
    ui_text(ui, x + w * 0.62f + S(ui, 12), y - (float)ui->line_h - S(ui, 4), T("vps.wgport"), COL_DIM);
    ui_textfield(ui, a->wg_port, (SDL_FRect){ x + w * 0.62f + S(ui, 12), y, w * 0.38f - S(ui, 12), fh }, a->wg_port, sizeof a->wg_port, "51820", !busy);
    y += fh + S(ui, 16);

    build_ports(a, udp, sizeof udp, tcp, sizeof tcp, &nu, &nt, why, sizeof why);
    two_buttons(a, x, y, w, bh, "vps_check", "vps_setup", T("vps.check"), T("vps.setup"), !busy, BTN_NORMAL, BTN_PRIMARY, &c1, &c2);
    if (c1) { const char *args[] = { "vps-check", NULL }; run_cli(a, T("vps.check.task"), args); }
    if (c2) {
        if (!ports_ok) log_line(a, T("ports.err.fix"), COL_RED);
        else if ((*a->public_ip && !safe_field(a->public_ip, ".")) || (*a->wg_port && !safe_field(a->wg_port, "")))
            log_line(a, T("vps.badfield"), COL_RED);
        else {
            const char *args[12];
            int k = 0;
            args[k++] = "vps-setup";
            args[k++] = "-udp"; args[k++] = *udp ? udp : "-";
            args[k++] = "-tcp"; args[k++] = *tcp ? tcp : "-";
            if (*a->wg_port) { args[k++] = "-wg-port"; args[k++] = a->wg_port; }
            if (*a->public_ip) { args[k++] = "-public-ip"; args[k++] = a->public_ip; }
            args[k] = NULL;
            run_cli(a, T("vps.setup.task"), args);
        }
    }
    y += bh + S(ui, 10);
    two_buttons(a, x, y, w, bh, "vps_status", "vps_ports", T("vps.status"), T("vps.ports"), !busy, BTN_NORMAL, BTN_NORMAL, &c1, &c2);
    if (c1) { const char *args[] = { "vps-status", NULL }; run_cli(a, T("vps.status.task"), args); }
    if (c2) { const char *args[] = { "ports", NULL }; run_cli(a, T("vps.ports.task"), args); }
    y += bh + S(ui, 22);

    ui_fill(ui, (SDL_FRect){ x, y - S(ui, 10), w, S(ui, 1) }, COL_BORDER);
    ui_checkbox(ui, &a->purge, x, y, T("vps.purge"), &a->purge, !busy);
    ui_checkbox(ui, &a->disable_routing, x + w / 2, y, T("vps.disablerouting"), &a->disable_routing, !busy);
    y += (float)ui->line_h + S(ui, 10);
    if (ui_button(ui, "vps_uninstall", (SDL_FRect){ x, y, w, bh }, T("vps.uninstall"), !busy, BTN_DANGER)) {
        if (confirm(a, T("vps.uninstall"), T("vps.uninstall.confirm"))) {
            const char *args[6];
            int k = 0;
            args[k++] = "vps-uninstall"; args[k++] = "-force";
            if (a->purge) args[k++] = "-purge";
            if (a->disable_routing) args[k++] = "-disable-routing";
            args[k] = NULL;
            run_cli(a, T("vps.uninstall.task"), args);
        }
    }
    y += bh + S(ui, 12);
    wrap_text(a, x, y, w, T("vps.note"), COL_DIM);
}

static void draw_gamepc(App *a, SDL_FRect r, bool busy, bool ports_ok)
{
    UI *ui = &a->ui;
    float x = r.x + S(ui, 14), y = r.y + S(ui, 12), w = r.w - S(ui, 28), fh = S(ui, 34), bh = S(ui, 38), bw = S(ui, 96);
    char udp[2048], tcp[2048], why[160];
    int nu, nt;
    bool c1, c2;

    ui_fill(ui, r, COL_PANEL);
    ui_text_bold(ui, x, y, T("gamepc.title"), COL_TEXT);
    y += (float)ui->line_h + S(ui, 12);
    y = label(a, x, y, T("gamepc.conf"));
    ui_textfield(ui, a->conf_path, (SDL_FRect){ x, y, w - bw - S(ui, 8), fh }, a->conf_path, sizeof a->conf_path, T("gamepc.conf.hint"), !busy);
    if (ui_button(ui, "browse", (SDL_FRect){ x + w - bw, y, bw, fh }, T("gamepc.browse"), !busy, BTN_NORMAL)) {
        const SDL_DialogFileFilter filters[] = { { T("gamepc.filter.conf"), "conf" }, { T("gamepc.filter.all"), "*" } };
        SDL_ShowOpenFileDialog(on_file_chosen, a, ui->win, filters, 2, NULL, false);
    }
    y += fh + S(ui, 16);

    build_ports(a, udp, sizeof udp, tcp, sizeof tcp, &nu, &nt, why, sizeof why);
    two_buttons(a, x, y, w, bh, "gp_install", "gp_up", T("gamepc.install"), T("gamepc.up"), !busy, BTN_PRIMARY, BTN_PRIMARY, &c1, &c2);
    if (c1) {
        if (!*a->conf_path) log_line(a, T("gamepc.noconf"), COL_RED);
        else if (SDL_strchr(a->conf_path, '"')) log_line(a, T("gamepc.badpath"), COL_RED);
        else if (!ports_ok) log_line(a, T("ports.err.fix"), COL_RED);
        else {
            const char *args[] = { "gamepc-install", "-config", a->conf_path, "-udp", *udp ? udp : "-", "-tcp", *tcp ? tcp : "-", NULL };
            run_cli(a, T("gamepc.install.task"), args);
        }
    }
    if (c2) { const char *args[] = { "gamepc-up", NULL }; run_cli(a, T("gamepc.up.task"), args); }
    y += bh + S(ui, 10);
    two_buttons(a, x, y, w, bh, "gp_down", "gp_status", T("gamepc.down"), T("gamepc.status"), !busy, BTN_NORMAL, BTN_NORMAL, &c1, &c2);
    if (c1) { const char *args[] = { "gamepc-down", NULL }; run_cli(a, T("gamepc.down.task"), args); }
    if (c2) { const char *args[] = { "gamepc-status", NULL }; run_cli(a, T("gamepc.status.task"), args); }
    y += bh + S(ui, 22);

    ui_fill(ui, (SDL_FRect){ x, y - S(ui, 10), w, S(ui, 1) }, COL_BORDER);
    ui_checkbox(ui, &a->keep_config, x, y, T("gamepc.keepconfig"), &a->keep_config, !busy);
    y += (float)ui->line_h + S(ui, 10);
    if (ui_button(ui, "gamepc_uninstall", (SDL_FRect){ x, y, w, bh }, T("gamepc.uninstall"), !busy, BTN_DANGER)) {
        if (confirm(a, T("gamepc.uninstall"), T("gamepc.uninstall.confirm"))) {
            const char *args[] = { "gamepc-uninstall", "-force", a->keep_config ? "-keep-config" : NULL, NULL };
            run_cli(a, T("gamepc.uninstall.task"), args);
        }
    }
    y += bh + S(ui, 12);
    wrap_text(a, x, y, w, T("gamepc.note"), COL_DIM);
}

static void draw_verify(App *a, SDL_FRect r, bool busy)
{
    UI *ui = &a->ui;
    float x = r.x + S(ui, 14), y = r.y + S(ui, 12), w = SDL_min(r.w - S(ui, 28), S(ui, 640)), fh = S(ui, 34), bh = S(ui, 38);

    ui_fill(ui, r, COL_PANEL);
    ui_text_bold(ui, x, y, T("verify.title"), COL_TEXT);
    y += (float)ui->line_h + S(ui, 12);
    y = label(a, x, y, T("verify.target"));
    ui_textfield(ui, a->target, (SDL_FRect){ x, y, w, fh }, a->target, sizeof a->target, T("verify.target.hint"), !busy);
    y += fh + S(ui, 10);
    y = label(a, x, y, T("verify.qport"));
    ui_textfield(ui, a->qport, (SDL_FRect){ x, y, S(ui, 160), fh }, a->qport, sizeof a->qport, "27015", !busy);
    y += fh + S(ui, 10);
    y = label(a, x, y, T("verify.steamkey"));
    ui_textfield(ui, a->steam_key, (SDL_FRect){ x, y, w, fh }, a->steam_key, sizeof a->steam_key, "https://steamcommunity.com/dev/apikey", !busy);
    y += fh + S(ui, 16);
    if (ui_button(ui, "verify", (SDL_FRect){ x, y, S(ui, 200), bh }, T("verify.run"), !busy, BTN_PRIMARY)) {
        if (!*a->target || !safe_field(a->target, ".-") || (*a->qport && !safe_field(a->qport, "")) || (*a->steam_key && !safe_field(a->steam_key, "")))
            log_line(a, T("verify.badfield"), COL_RED);
        else {
            const char *args[10];
            int k = 0;
            args[k++] = "verify"; args[k++] = "-target"; args[k++] = a->target;
            if (*a->qport) { args[k++] = "-query-port"; args[k++] = a->qport; }
            if (*a->steam_key) { args[k++] = "-steam-key"; args[k++] = a->steam_key; }
            args[k] = NULL;
            run_cli(a, T("verify.task"), args);
        }
    }
    if (ui_button(ui, "wg_install", (SDL_FRect){ x + S(ui, 210), y, S(ui, 220), bh }, T("verify.wginstall"), !busy, BTN_NORMAL)) {
        const char *args[] = { "install-wireguard", NULL };
        run_cli(a, T("verify.wginstall.task"), args);
    }
#ifdef _WIN32
    y += bh + S(ui, 22);
    ui_text_bold(ui, x, y, T("verify.wintools"), COL_TEXT);
    y += (float)ui->line_h + S(ui, 8);
    wrap_text(a, x, y, w, T("verify.winnat.note"), COL_DIM);
    y += 3 * ((float)ui->line_h + S(ui, 2)) + S(ui, 6);
    if (ui_button(ui, "winnat_enable", (SDL_FRect){ x, y, S(ui, 320), bh }, T("verify.winnat.btn"), !busy, BTN_NORMAL)) {
        if (confirm(a, T("verify.winnat.task"), T("verify.winnat.confirm"))) {
            const char *args[] = { "winnat-enable", "-force", NULL };
            run_cli(a, T("verify.winnat.task"), args);
        }
    }
#endif
}

static void draw_log(App *a, SDL_FRect r)
{
    UI *ui = &a->ui;
    float x = r.x + S(ui, 12), hy = r.y + S(ui, 10), bw = S(ui, 76), bh = S(ui, 28);
    SDL_FRect body;
    float lh = (float)ui->line_h + S(ui, 2), total = 0, y;
    int tw = (int)(r.w - S(ui, 36));

    ui_fill(ui, r, COL_PANEL);
    ui_text_bold(ui, x, hy, T("log.title"), COL_TEXT);
    if (a->status[0]) ui_text(ui, x + S(ui, 110), hy + S(ui, 2), a->status, a->status_col);
    if (ui_button(ui, "log_clear", (SDL_FRect){ r.x + r.w - bw - S(ui, 12), hy - S(ui, 2), bw, bh }, T("log.clear"), true, BTN_NORMAL)) log_clear(a);
    if (ui_button(ui, "log_copy", (SDL_FRect){ r.x + r.w - 2 * bw - S(ui, 20), hy - S(ui, 2), bw, bh }, T("log.copy"), a->nlines > 0, BTN_NORMAL)) log_copy(a);

    body = (SDL_FRect){ r.x + S(ui, 8), r.y + S(ui, 44), r.w - S(ui, 16), r.h - S(ui, 52) };
    ui_fill(ui, body, COL_INPUT);

    /* 줄바꿈된 줄 수 계산 (너비가 바뀐 줄만) */
    for (int i = 0; i < a->nlines; i++) {
        LogLine *L = &a->lines[i];
        if (L->rows_w != tw) {
            size_t len = SDL_strlen(L->text), pos = 0;
            int rows = 0;
            while (pos < len) { size_t n = ui_fit(ui, L->text + pos, len - pos, tw); if (!n) n = 1; pos += n; rows++; }
            L->rows = rows ? rows : 1;
            L->rows_w = tw;
        }
        total += (float)L->rows * lh;
    }
    {
        float maxs = SDL_max(0.0f, total - (body.h - S(ui, 12)));
        if (ui_hit(ui, body) && ui->wheel != 0) { a->scroll -= ui->wheel * lh * 3; a->follow = false; }
        if (a->follow) a->scroll = maxs;
        if (a->scroll < 0) a->scroll = 0;
        if (a->scroll >= maxs) { a->scroll = maxs; a->follow = true; }
    }

    SDL_SetRenderClipRect(ui->ren, &(SDL_Rect){ (int)body.x, (int)body.y, (int)body.w, (int)body.h });
    y = body.y + S(ui, 6) - a->scroll;
    if (a->nlines == 0) ui_text(ui, body.x + S(ui, 10), body.y + S(ui, 8), T("log.empty"), COL_DIM);
    for (int i = 0; i < a->nlines; i++) {
        LogLine *L = &a->lines[i];
        float h = (float)L->rows * lh;
        if (y + h >= body.y && y <= body.y + body.h) {
            size_t len = SDL_strlen(L->text), pos = 0;
            float yy = y;
            char seg[1024];
            while (pos < len) {
                size_t n = ui_fit(ui, L->text + pos, len - pos, tw);
                if (!n) n = 1;
                if (n >= sizeof seg) n = sizeof seg - 1;
                SDL_memcpy(seg, L->text + pos, n);
                seg[n] = 0;
                ui_text(ui, body.x + S(ui, 10), yy, seg, L->color);
                yy += lh;
                pos += n;
            }
        }
        y += h;
    }
    SDL_SetRenderClipRect(ui->ren, NULL);
}

static void draw(App *a)
{
    UI *ui = &a->ui;
    int W, H;
    bool busy = runner_busy(&a->run), ports_ok;
    float tabs_y, top, split, m = S(ui, 14);
    static const char *const tab_ids[] = { "tab0", "tab1", "tab2" };
    const char *tabs[3];
    char note[256];

    tabs[0] = T("tab.vps"); tabs[1] = T("tab.gamepc"); tabs[2] = T("tab.verify");
    SDL_GetCurrentRenderOutputSize(ui->ren, &W, &H);
    SDL_SetRenderDrawColor(ui->ren, COL_BG.r, COL_BG.g, COL_BG.b, 255);
    SDL_RenderClear(ui->ren);

    /* 머리글 */
    ui_text_bold(ui, m, S(ui, 14), APP_TITLE, COL_TEXT);
#ifdef _WIN32
    SDL_snprintf(note, sizeof note, "%s", T("env.win"));
#else
    SDL_snprintf(note, sizeof note, T("env.linux"), a->elevated ? T("env.linux.root") : (a->use_pkexec ? T("env.linux.pkexec") : T("env.linux.noroot")));
#endif
    {
        int w, h;
        ui_text_size(ui, note, &w, &h);
        ui_text(ui, (float)W - m - (float)w, S(ui, 16), note, COL_DIM);
        if (!a->cli_ok) {
            const char *warn = T("cli.missing.short");
            ui_text_size(ui, warn, &w, &h);
            ui_text(ui, (float)W - m - (float)w, S(ui, 16) + (float)h, warn, COL_RED);
        }
        /* Language: one button that cycles through whatever the lang folder provides. */
        if (i18n_count() > 1) {
            float bw = S(ui, 120), bh2 = S(ui, 26);
            if (ui_button(ui, "lang_cycle", (SDL_FRect){ (float)W - m - (float)w - bw - S(ui, 14), S(ui, 13), bw, bh2 },
                          i18n_name(a->lang_idx), true, BTN_NORMAL))
                lang_next(a);
        }
    }

    /* 탭 */
    tabs_y = S(ui, 54);
    for (int i = 0; i < 3; i++)
        if (ui_tab(ui, tab_ids[i], (SDL_FRect){ m + (float)i * S(ui, 168), tabs_y, S(ui, 164), S(ui, 38) }, tabs[i], a->tab == i)) a->tab = i;
    top = tabs_y + S(ui, 38);
    split = SDL_max(top + S(ui, 470), (float)H * 0.64f);
    if (split > (float)H - S(ui, 150)) split = (float)H - S(ui, 150);

    if (a->tab == 2) {
        draw_verify(a, (SDL_FRect){ m, top, (float)W - 2 * m, split - top }, busy);
    } else {
        float lw = SDL_min(S(ui, 520), ((float)W - 3 * m) * 0.5f);
        char u[2048], t[2048], why[160];
        int nu, nt;
        ports_ok = build_ports(a, u, sizeof u, t, sizeof t, &nu, &nt, why, sizeof why);
        if (a->tab == 0) draw_vps(a, (SDL_FRect){ m, top, (float)W - 3 * m - lw, split - top }, busy, ports_ok);
        else draw_gamepc(a, (SDL_FRect){ m, top, (float)W - 3 * m - lw, split - top }, busy, ports_ok);
        draw_ports_panel(a, (SDL_FRect){ (float)W - m - lw, top, lw, split - top }, busy);
    }
    draw_log(a, (SDL_FRect){ m, split + S(ui, 10), (float)W - 2 * m, (float)H - split - S(ui, 10) - m });
}

/* ------------------------------------------------------------------ 시작 */
static void find_cli(App *a)
{
    const char *base = SDL_GetBasePath();
#ifdef _WIN32
    const char *name = "tgui.exe";
#else
    const char *name = "tgui";
    static const char *const sys[] = { "/usr/local/bin/tgui", "/usr/bin/tgui" };
#endif
    SDL_PathInfo info;
    SDL_snprintf(a->cli, sizeof a->cli, "%s%s", base ? base : "", name);
    a->cli_ok = SDL_GetPathInfo(a->cli, &info) && info.type == SDL_PATHTYPE_FILE;
#ifndef _WIN32
    for (size_t i = 0; !a->cli_ok && i < SDL_arraysize(sys); i++) {
        SDL_strlcpy(a->cli, sys[i], sizeof a->cli);
        a->cli_ok = SDL_GetPathInfo(a->cli, &info) && info.type == SDL_PATHTYPE_FILE;
    }
#endif
}

static void init_env(App *a)
{
    SDL_PathInfo info;
#ifdef _WIN32
    const char *pd = SDL_getenv("ProgramData");
    SDL_snprintf(a->settings_file, sizeof a->settings_file, "%s\\TunnelingGUI\\settings.conf", pd ? pd : "C:\\ProgramData");
    a->elevated = true;   /* 매니페스트(requireAdministrator)로 실행된다 */
#else
    SDL_strlcpy(a->settings_file, "/etc/tgui/settings.conf", sizeof a->settings_file);
    a->elevated = geteuid() == 0;
    a->use_pkexec = (SDL_GetPathInfo("/usr/bin/pkexec", &info) && info.type == SDL_PATHTYPE_FILE) ||
                    (SDL_GetPathInfo("/bin/pkexec", &info) && info.type == SDL_PATHTYPE_FILE);
#endif
    (void)info;
    find_cli(a);
}

/* Language files live in lang/ next to the executable. English is built in, so a
   missing folder is not an error. The choice is remembered per user.
   Order of precedence: --lang argument > TGUI_LANG > saved choice > English. */
static void lang_init(App *a, const char *arg_lang)
{
    const char *base = SDL_GetBasePath();
    const char *pref = SDL_GetPrefPath("TunnelingGUI", "TunnelingGUI");
    char dir[1100], saved[16] = "";
    const char *want = NULL;
    int n = 0;
    char **found;

    SDL_snprintf(dir, sizeof dir, "%slang", base ? base : "");
    i18n_init(dir, NULL);                       /* English; registers what it can find */

    /* Any lang/<code>.json counts, so adding a language needs no code change. */
    found = SDL_GlobDirectory(dir, "*.json", 0, &n);
    if (found) {
        for (int i = 0; i < n; i++) {
            char code[32];
            const char *dot = SDL_strrchr(found[i], '.');
            size_t len = dot ? (size_t)(dot - found[i]) : SDL_strlen(found[i]);
            if (len == 0 || len >= sizeof code) continue;
            SDL_memcpy(code, found[i], len);
            code[len] = 0;
            i18n_add_file(code);
        }
        SDL_free(found);
    }

    a->lang_file[0] = 0;
    if (pref) {
        size_t len = 0;
        char *t;
        SDL_snprintf(a->lang_file, sizeof a->lang_file, "%slang.txt", pref);
        if ((t = (char *)SDL_LoadFile(a->lang_file, &len)) != NULL) {
            size_t i = 0;
            while (i < len && i + 1 < sizeof saved && t[i] > ' ') { saved[i] = t[i]; i++; }
            saved[i] = 0;
            SDL_free(t);
        }
    }

    if (arg_lang && *arg_lang) want = arg_lang;
    else if (SDL_getenv("TGUI_LANG")) want = SDL_getenv("TGUI_LANG");
    else if (*saved) want = saved;

    if (want && *want) i18n_set(want);
    for (int i = 0; i < i18n_count(); i++)
        if (!SDL_strcmp(i18n_code(i), i18n_current())) { a->lang_idx = i; break; }
}

/* Cycle to the next available language and remember it. */
static void lang_next(App *a)
{
    int n = i18n_count();
    if (n < 2) return;
    for (int step = 1; step <= n; step++) {
        int i = (a->lang_idx + step) % n;
        if (i18n_set(i18n_code(i))) {
            a->lang_idx = i;
            if (a->lang_file[0]) SDL_SaveFile(a->lang_file, i18n_code(i), SDL_strlen(i18n_code(i)));
            return;
        }
    }
}

static void demo_content(App *a)
{
    SDL_strlcpy(a->extra_udp, "7779-7782,27016-27017", sizeof a->extra_udp);
    SDL_strlcpy(a->public_ip, "203.0.113.5", sizeof a->public_ip);
    SDL_strlcpy(a->conf_path, "C:\\Users\\Example\\Downloads\\client-gamepc.conf", sizeof a->conf_path);
    a->have_saved = true;
    SDL_strlcpy(a->saved_role, "vps", sizeof a->saved_role);
    SDL_strlcpy(a->saved_udp, "7777,7778,27015", sizeof a->saved_udp);
    log_stream(a, "$ tgui vps-setup -udp 7779-7782,27015-27017 -tcp - -public-ip 203.0.113.5\n"
                  "==> Pre-flight check\n  PowerShell: PS 5.1 / 64bit-process=True\n==> Keys\n  reusing existing keys\n"
                  "==> Installing the WireGuard server tunnel\n  interface tgui-gw is up\n==> Configuring WinNAT\n  UDP 7779-7782,27015-27017 -> 10.8.0.2 (same ports)\n"
                  "Warning: open the same ports in your cloud firewall.\nDone.\n", true);
    SDL_strlcpy(a->status, T("demo.status"), sizeof a->status);
    a->status_col = COL_GREEN;
}

int main(int argc, char **argv)
{
    App *a;
    char font[1024];
    const char *shot = NULL, *exec_args = NULL, *arg_lang = NULL;
    bool demo = false, quit = false, exec_done = false;
    int frames = 0;

    for (int i = 1; i < argc; i++) {
        if (!SDL_strcmp(argv[i], "--screenshot") && i + 1 < argc) shot = argv[++i];
        else if (!SDL_strcmp(argv[i], "--demo")) demo = true;
        else if (!SDL_strcmp(argv[i], "--exec") && i + 1 < argc) exec_args = argv[++i];   /* 테스트용: 시작하자마자 CLI 실행 */
        else if (!SDL_strcmp(argv[i], "--lang") && i + 1 < argc) arg_lang = argv[++i];
    }

    SDL_SetAppMetadata(APP_TITLE, TGUI_VERSION, "com.tunnelinggui.app");
    if (!SDL_Init(SDL_INIT_VIDEO)) { SDL_Log("SDL_Init: %s", SDL_GetError()); return 1; }
    if (!TTF_Init()) { SDL_Log("TTF_Init: %s", SDL_GetError()); return 1; }

    a = (App *)SDL_calloc(1, sizeof *a);
    if (!a) return 1;
    a->dlg_lock = SDL_CreateMutex();
    lang_init(a, arg_lang);
    runner_init(&a->run);
    SDL_strlcpy(a->wg_port, "51820", sizeof a->wg_port);
    SDL_strlcpy(a->qport, "27015", sizeof a->qport);
    a->p7777 = a->p7778 = a->p27015 = true;
    a->follow = true;
    init_env(a);
    load_settings(a, true);
    for (int i = 1; i < argc; i++) if (!SDL_strcmp(argv[i], "--tab") && i + 1 < argc) a->tab = SDL_atoi(argv[++i]) % 3;
    if (demo) demo_content(a);

    {
        SDL_Window *win = NULL;
        SDL_Renderer *ren = NULL;
        if (!SDL_CreateWindowAndRenderer(APP_TITLE, 1100, 820, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &win, &ren)) {
            SDL_Log("window: %s", SDL_GetError());
            return 1;
        }
        SDL_SetWindowMinimumSize(win, 900, 680);
        SDL_SetRenderVSync(ren, 1);
        if (!find_korean_font(font, sizeof font)) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, APP_TITLE,
                T("font.missing"), win);
            return 1;
        }
        if (!ui_init(&a->ui, win, ren, font)) { SDL_Log("font: %s", SDL_GetError()); return 1; }
    }
    if (i18n_rejected() > 0) {
        char msg[160];
        SDL_snprintf(msg, sizeof msg, T("i18n.rejected"), i18n_rejected());
        log_line(a, msg, COL_YELLOW);
    }
    if (!a->cli_ok) log_line(a, T("cli.missing"), COL_RED);
    if (exec_args) {
        static char buf[1024];
        const char *args[24];
        char *save = NULL;
        int k = 0;
        SDL_strlcpy(buf, exec_args, sizeof buf);
        for (char *t = SDL_strtok_r(buf, " ", &save); t && k < 23; t = SDL_strtok_r(NULL, " ", &save)) args[k++] = t;
        args[k] = NULL;
        run_cli(a, T("exec.task"), args);
    }

    while (!quit) {
        SDL_Event e;
        bool busy = runner_busy(&a->run);
        ui_begin(&a->ui);
        if (SDL_WaitEventTimeout(&e, busy ? 30 : 250)) {
            do {
                if (e.type == SDL_EVENT_QUIT) {
                    if (runner_busy(&a->run))
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, APP_TITLE, T("quit.busy"), a->ui.win);
                    quit = true;
                }
                ui_event(&a->ui, &e);
            } while (SDL_PollEvent(&e));
        }

        /* 실행 결과 받기 */
        {
            bool done = false;
            int code = 0;
            char *out = runner_take(&a->run, &done, &code);
            if (out) { log_stream(a, out, false); SDL_free(out); }
            if (done) {
                exec_done = true;
                log_stream(a, "", true);
                if (code == 0) { SDL_snprintf(a->status, sizeof a->status, T("cli.done"), a->run.title); a->status_col = COL_GREEN; }
                else { SDL_snprintf(a->status, sizeof a->status, T("cli.failed"), a->run.title, code); a->status_col = COL_RED; }
                load_settings(a, false);   /* 적용된 포트 표시 갱신 */
            }
        }
        /* 파일 선택 결과 */
        SDL_LockMutex(a->dlg_lock);
        if (a->dlg_ready) { SDL_strlcpy(a->conf_path, a->dlg_path, sizeof a->conf_path); a->dlg_ready = false; }
        SDL_UnlockMutex(a->dlg_lock);

        draw(a);
        ui_end(&a->ui);

        if (shot && (!exec_args || exec_done) && ++frames >= 3) {
            SDL_Surface *s = SDL_RenderReadPixels(a->ui.ren, NULL);
            if (!s || !SDL_SavePNG(s, shot)) SDL_Log("screenshot: %s", SDL_GetError());
            SDL_DestroySurface(s);
            quit = true;
        }
        SDL_RenderPresent(a->ui.ren);
    }

    runner_quit(&a->run);
    log_clear(a);
    ui_quit(&a->ui);
    SDL_DestroyRenderer(a->ui.ren);
    SDL_DestroyWindow(a->ui.win);
    SDL_DestroyMutex(a->dlg_lock);
    SDL_free(a);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
