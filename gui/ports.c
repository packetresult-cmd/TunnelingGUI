/* ports.c - GUI 쪽 포트 목록 처리 (화면에서 미리 검증하고 개수를 보여주기 위해) */
#include <SDL3/SDL.h>
#include "ports.h"

static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

bool ports_has(const PortSet *ps, int port)
{
    for (int i = 0; i < ps->n; i++) if (ps->v[i] == port) return true;
    return false;
}

bool ports_add(PortSet *ps, int port)
{
    if (port < 1 || port > 65535) return false;
    if (ports_has(ps, port)) return true;
    if (ps->n >= GUI_MAX_PORTS) return false;
    ps->v[ps->n++] = port;
    SDL_qsort(ps->v, (size_t)ps->n, sizeof(int), cmp_int);
    return true;
}

bool ports_add_list(PortSet *ps, const char *s, char *why, size_t wsz)
{
    const char *p = s;
    while (*p) {
        long lo, hi;
        char *end;
        while (*p == ' ' || *p == ',') p++;
        if (!*p) break;
        if (*p < '0' || *p > '9') { SDL_snprintf(why, wsz, "숫자가 아닌 글자: '%c'", *p); return false; }
        lo = SDL_strtol(p, &end, 10);
        hi = lo;
        if (*end == '-') {
            if (end[1] < '0' || end[1] > '9') { SDL_snprintf(why, wsz, "범위 끝이 없습니다"); return false; }
            hi = SDL_strtol(end + 1, &end, 10);
        }
        if (*end && *end != ',' && *end != ' ') { SDL_snprintf(why, wsz, "형식 오류 (예: 7777,7779-7780)"); return false; }
        if (lo < 1 || hi > 65535 || lo > hi) { SDL_snprintf(why, wsz, "포트 범위 오류: %ld-%ld (1~65535, 작은 수-큰 수)", lo, hi); return false; }
        for (long v = lo; v <= hi; v++)
            if (!ports_add(ps, (int)v)) { SDL_snprintf(why, wsz, "포트가 너무 많습니다 (최대 %d개)", GUI_MAX_PORTS); return false; }
        p = end;
    }
    return true;
}

void ports_format(const PortSet *ps, char *out, size_t sz)
{
    out[0] = 0;
    for (int i = 0; i < ps->n;) {
        int j = i;
        char t[24];
        while (j + 1 < ps->n && ps->v[j + 1] == ps->v[j] + 1) j++;
        if (j - i >= 2) SDL_snprintf(t, sizeof t, "%s%d-%d", *out ? "," : "", ps->v[i], ps->v[j]);
        else { j = i; SDL_snprintf(t, sizeof t, "%s%d", *out ? "," : "", ps->v[i]); }
        SDL_strlcat(out, t, sz);
        i = j + 1;
    }
}
