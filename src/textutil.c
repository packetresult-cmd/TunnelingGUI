/* textutil.c
 * 입력 검증, 포트 목록, JSON/conf 값 읽기.
 */
#include "tgui.h"

void trim(char *s)
{
    size_t n = strlen(s), a = 0;
    while (n && (s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == ' ' || s[n - 1] == '\t')) s[--n] = 0;
    while (s[a] == ' ' || s[a] == '\t') a++;
    if (a) memmove(s, s + a, strlen(s + a) + 1);
}

BOOL valid_ip(const char *s)
{
    struct in_addr a;
    return inet_pton(AF_INET, s, &a) == 1;
}

BOOL valid_subnet(const char *s)
{
    char ip[32], *sl;
    int n;
    snprintf(ip, sizeof ip, "%s", s);
    sl = strchr(ip, '/');
    if (!sl) return FALSE;
    *sl = 0;
    n = atoi(sl + 1);
    return valid_ip(ip) && n >= 8 && n <= 30;
}

static int cmp_int(const void *x, const void *y)
{
    return *(const int *)x - *(const int *)y;
}

/* "7777,7778,27015" 또는 범위 "7777-7790,27015" 를 개별 포트 배열로 (중복 제거, 오름차순).
 * 빈 문자열은 실패. 포트 0 개를 뜻하려면 호출하는 쪽에서 "-" 를 따로 처리한다 */
BOOL parse_ports(const char *s, int *arr, int *n)
{
    char b[PORTS_STR_MAX], *tok, *save = NULL;
    int c = 0;
    if (strlen(s) >= sizeof b) return FALSE;
    snprintf(b, sizeof b, "%s", s);
    for (tok = strtok_r(b, ", ", &save); tok; tok = strtok_r(NULL, ", ", &save)) {
        char *end;
        long lo = strtol(tok, &end, 10), hi = lo;
        if (*end == '-') hi = strtol(end + 1, &end, 10);
        if (*end || lo < 1 || hi > 65535 || lo > hi) return FALSE;
        for (long p = lo; p <= hi; p++) {
            int dup = 0;
            for (int i = 0; i < c; i++) if (arr[i] == (int)p) { dup = 1; break; }
            if (dup) continue;
            if (c >= MAX_PORTS) return FALSE;
            arr[c++] = (int)p;
        }
    }
    if (!c) return FALSE;
    qsort(arr, (size_t)c, sizeof *arr, cmp_int);
    *n = c;
    return TRUE;
}

/* 보기 좋은 형태: 연속 3개 이상은 범위로 묶는다 ("7777-7790,27015") */
void ports_str(const int *a, int n, char *out, size_t sz)
{
    out[0] = 0;
    for (int i = 0; i < n;) {
        int j = i;
        char t[24];
        while (j + 1 < n && a[j + 1] == a[j] + 1) j++;
        if (j - i >= 2) snprintf(t, sizeof t, "%s%d-%d", *out ? "," : "", a[i], a[j]);
        else { j = i; snprintf(t, sizeof t, "%s%d", *out ? "," : "", a[i]); }
        strncat(out, t, sz - strlen(out) - 1);
        i = j + 1;
    }
}

/* 모든 포트를 하나씩 나열 ("7777,7778,...") - PowerShell foreach 등 범위를 모르는 곳에 쓴다 */
void ports_plain(const int *a, int n, char *out, size_t sz)
{
    out[0] = 0;
    for (int i = 0; i < n; i++) {
        char t[16];
        snprintf(t, sizeof t, "%s%d", i ? "," : "", a[i]);
        strncat(out, t, sz - strlen(out) - 1);
    }
}

BOOL valid_dns(const char *s)
{
    if (!*s) return FALSE;
    for (; *s; s++)
        if (!((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f') || *s == '.' || *s == ':' || *s == ',' || *s == ' ')) return FALSE;
    return TRUE;
}
BOOL json_val(const char *t, const char *section, const char *key, char *out, size_t sz)
{
    char pat[64];
    const char *p, *q;
    size_t n;
    snprintf(pat, sizeof pat, "\"%s\"", section);
    if (!(p = strstr(t, pat))) return FALSE;
    snprintf(pat, sizeof pat, "\"%s\"", key);
    if (!(p = strstr(p, pat))) return FALSE;
    if (!(p = strchr(p + strlen(pat), ':'))) return FALSE;
    if (!(p = strchr(p, '"'))) return FALSE;
    p++;
    if (!(q = strchr(p, '"'))) return FALSE;
    n = (size_t)(q - p);
    if (n == 0 || n >= sz) return FALSE;
    memcpy(out, p, n);
    out[n] = 0;
    return TRUE;
}

BOOL conf_get(const char *text, const char *key, char *out, size_t sz)
{
    const char *p = text;
    size_t kl = strlen(key);
    while (p && *p) {
        const char *line = p;
        while (*line == ' ' || *line == '\t') line++;
        if (!_strnicmp(line, key, kl)) {
            const char *q = line + kl;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == '=') {
                const char *e;
                q++;
                while (*q == ' ' || *q == '\t') q++;
                e = q;
                while (*e && *e != '\r' && *e != '\n') e++;
                if ((size_t)(e - q) >= sz) return FALSE;
                memcpy(out, q, (size_t)(e - q));
                out[e - q] = 0;
                trim(out);
                return TRUE;
            }
        }
        p = strchr(p, '\n');
        if (p) p++;
    }
    return FALSE;
}
