/* wireguard.c
 * 플랫폼과 무관한 WireGuard 보조 함수: 자동 설치 호출, 키 생성, 핸드셰이크 조회.
 */
#include "tgui.h"

BOOL ensure_wireguard(void)
{
    if (wg_installed()) return TRUE;
    if (O.no_auto_install) { say(C_RED, "WireGuard 가 설치되어 있지 않습니다. (-no-auto-install)\n"); return FALSE; }
    pf("WireGuard 가 없어 자동으로 설치합니다.\n");
    return install_wireguard();
}

BOOL gen_keypair(KeyPair *k)
{
    char cmd[MAX_PATH + 16], out[256];
    DWORD code = 1;
    snprintf(cmd, sizeof cmd, "\"%s\" genkey", g_wg);
    if (!run(cmd, NULL, out, sizeof out, &code) || code) return FALSE;
    trim(out);
    snprintf(k->priv, sizeof k->priv, "%.63s", out);
    snprintf(cmd, sizeof cmd, "\"%s\" pubkey", g_wg);
    if (!run(cmd, k->priv, out, sizeof out, &code) || code) return FALSE;
    trim(out);
    snprintf(k->pub, sizeof k->pub, "%.63s", out);
    return *k->priv && *k->pub;
}

/* 마지막 핸드셰이크 이후 초. 없으면 -1 */
long handshake_age(const char *tunnel)
{
    char cmd[MAX_PATH + 64], out[512];
    DWORD code = 1;
    char *p;
    long long epoch;
    snprintf(cmd, sizeof cmd, "\"%s\" show %s latest-handshakes", g_wg, tunnel);
    if (!run(cmd, NULL, out, sizeof out, &code) || code != 0) return -1;
    p = strpbrk(out, " \t");
    if (!p) return -1;
    epoch = _strtoi64(p + 1, NULL, 10);
    if (epoch <= 0) return -1;
    return (long)((long long)time(NULL) - epoch);
}
