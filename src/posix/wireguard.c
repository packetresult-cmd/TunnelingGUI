/* posix/wireguard.c
 * Linux WireGuard 설치 확인과 배포판 패키지 관리자를 통한 설치.
 */
#include "tgui.h"
#include "pkg.h"

BOOL wg_installed(void)
{
    return have_cmd("wg") && have_cmd("wg-quick");
}

BOOL install_wireguard(void)
{
    step("WireGuard 설치 (배포판 패키지: wireguard-tools)");
    if (!ensure_cmd("wg") || !wg_installed()) {
        say(C_RED, "WireGuard 설치 실패. 배포판 문서에 따라 wireguard-tools 를 직접 설치하세요.\n");
        return FALSE;
    }
    say(C_GREEN, "WireGuard 설치 완료\n");
    return TRUE;
}

/* 게임 PC 설정을 Linux 용으로 보정:
 *  - CR 제거 (Windows 에서 만든 conf 를 wg-quick 이 읽으면 값 끝에 CR 이 붙어 오동작)
 *  - resolvconf 가 없으면 DNS 줄 제거 (wg-quick 이 DNS= 처리에 resolvconf 를 호출함) */
char *adapt_client_conf(const char *text)
{
    size_t n = strlen(text);
    char *out = (char *)malloc(n + 1), *o = out;
    const char *p = text;
    BOOL has_resolv = have_cmd("resolvconf");
    BOOL dropped = FALSE;

    if (!out) return NULL;
    while (*p) {
        const char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) + 1 : strlen(p);
        const char *q = p;
        while (*q == ' ' || *q == '\t') q++;
        if (!has_resolv && !_strnicmp(q, "DNS", 3) && (q[3] == ' ' || q[3] == '=' || q[3] == '\t')) {
            dropped = TRUE;
        } else {
            for (size_t i = 0; i < len; i++) if (p[i] != '\r') *o++ = p[i];
        }
        p += len;
    }
    *o = 0;
    if (dropped) warn("resolvconf 가 없어 설정의 DNS 줄을 뺐습니다. (DNS 는 시스템 기본값 유지. 필요하면 resolvconf/openresolv 설치)");
    return out;
}
