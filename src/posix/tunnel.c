/* posix/tunnel.c
 * Linux WireGuard 터널: /etc/wireguard/<name>.conf + wg-quick@<name> (systemd).
 * systemd 가 없으면(컨테이너 등) wg-quick up/down 을 직접 호출한다(재부팅 후 자동 복구 안 됨).
 */
#include "tgui.h"
#include <net/if.h>

static void conf_path(const char *name, char *out, size_t sz)
{
    snprintf(out, sz, "%.900s/%.64s.conf", g_wgdir, name);
}

static BOOL use_systemd(void)
{
    return file_exists("/run/systemd/system");
}

static BOOL iface_up(const char *name)
{
    return if_nametoindex(name) != 0;
}

const char *tunnel_status(const char *name)
{
    char p[MAX_PATH];
    conf_path(name, p, sizeof p);
    if (!file_exists(p)) return NULL;
    return iface_up(name) ? "Running" : "Stopped";
}

BOOL tunnel_install(const char *name, const char *conf)
{
    char dst[MAX_PATH], cmd[512], out[2048];
    DWORD code = 1;
    char *text = read_file(conf);
    BOOL ok;

    if (!text) { say(C_RED, "설정 파일을 읽을 수 없습니다: %s\n", conf); return FALSE; }
    if (!make_dir(g_wgdir)) { free(text); say(C_RED, "폴더를 만들 수 없습니다: %s\n", g_wgdir); return FALSE; }
    conf_path(name, dst, sizeof dst);
    ok = write_file(dst, text);       /* 0600 */
    free(text);
    if (!ok) { say(C_RED, "설정 파일을 쓸 수 없습니다: %s\n", dst); return FALSE; }

    if (use_systemd()) {
        snprintf(cmd, sizeof cmd, "systemctl enable wg-quick@%s && systemctl restart wg-quick@%s", name, name);
    } else {
        warn("systemd 가 없어 wg-quick 으로 직접 켭니다. 재부팅 후에는 자동으로 켜지지 않습니다.");
        snprintf(cmd, sizeof cmd, "wg-quick up %s", name);
    }
    if (run(cmd, NULL, out, sizeof out, &code) && code == 0) return TRUE;

    trim(out);
    say(C_RED, "터널을 켜지 못했습니다: %.600s\n", out);
    tunnel_diagnose(name);
    pf("  WireGuard 커널 모듈(modprobe wireguard)과 설정 파일(%s)을 확인하세요.\n", dst);
    return FALSE;
}

BOOL tunnel_remove(const char *name)
{
    char p[MAX_PATH], cmd[256];
    int i;
    conf_path(name, p, sizeof p);
    if (!file_exists(p) && !iface_up(name)) return TRUE;

    if (use_systemd()) {
        snprintf(cmd, sizeof cmd, "systemctl disable --now wg-quick@%s", name);
        run_ok(cmd);
    }
    if (iface_up(name)) {            /* 서비스가 아니라 수동으로 올린 경우 */
        snprintf(cmd, sizeof cmd, "wg-quick down %s", name);
        run_ok(cmd);
    }
    for (i = 0; i < 20 && iface_up(name); i++) Sleep(250);
    if (iface_up(name)) return FALSE;
    unlink(p);
    return TRUE;
}

BOOL tunnel_wait_ready(const char *name, int timeout_ms)
{
    for (int waited = 0; waited <= timeout_ms; waited += 250) {
        if (iface_up(name)) return TRUE;
        Sleep(250);
    }
    return FALSE;
}

void tunnel_diagnose(const char *name)
{
    char cmd[256], out[4096];
    pf("  진단: 인터페이스 %s = %s\n", name, iface_up(name) ? "있음" : "없음");
    if (use_systemd()) snprintf(cmd, sizeof cmd, "journalctl -u wg-quick@%s -n 25 --no-pager 2>&1", name);
    else snprintf(cmd, sizeof cmd, "wg show %s 2>&1", name);
    if (run(cmd, NULL, out, sizeof out, NULL)) { trim(out); if (*out) pf("%s\n", out); }
}
