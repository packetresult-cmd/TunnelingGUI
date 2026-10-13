/* win/tunnel.c
 * WireGuard 터널 서비스와 네트워크 어댑터 (Windows).
 * 터널 서비스 이름: WireGuardTunnel$<name>
 */
#include "tgui.h"
#include <iphlpapi.h>

static BOOL svc_query(const char *name, DWORD *state)
{
    SC_HANDLE m = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT), s;
    SERVICE_STATUS st;
    char full[96];
    BOOL ok = FALSE;
    if (!m) return FALSE;
    snprintf(full, sizeof full, "WireGuardTunnel$%s", name);
    s = OpenServiceA(m, full, SERVICE_QUERY_STATUS);
    if (s) {
        ok = TRUE;
        if (state) *state = QueryServiceStatus(s, &st) ? st.dwCurrentState : 0;
        CloseServiceHandle(s);
    }
    CloseServiceHandle(m);
    return ok;
}

const char *tunnel_status(const char *name)
{
    DWORD st = 0;
    if (!svc_query(name, &st)) return NULL;
    switch (st) {
    case SERVICE_RUNNING: return "Running";
    case SERVICE_STOPPED: return "Stopped";
    case SERVICE_START_PENDING: return "StartPending";
    case SERVICE_STOP_PENDING: return "StopPending";
    default: return "Unknown";
    }
}

BOOL tunnel_remove(const char *name)
{
    char cmd[MAX_PATH + 64], out[2048];
    DWORD code = 1, st = 0;
    int i;
    if (!svc_query(name, NULL)) return TRUE;
    if (wg_installed()) snprintf(cmd, sizeof cmd, "\"%s\" /uninstalltunnelservice %s", g_wgexe, name);
    else snprintf(cmd, sizeof cmd, "sc.exe delete \"WireGuardTunnel$%s\"", name);   /* WireGuard 가 이미 지워진 경우 */
    if (!run(cmd, NULL, out, sizeof out, &code) || code != 0) {
        trim(out);
        say(C_RED, "터널 서비스 제거 명령 실패 (종료 코드 %lu): %s\n", (unsigned long)code, *out ? out : "(출력 없음)");
    }
    for (i = 0; i < 40 && svc_query(name, NULL); i++) Sleep(500);
    if (!svc_query(name, NULL)) return TRUE;
    /* 다른 프로그램(services.msc, WireGuard UI 등)이 서비스 핸들을 열고 있으면 "삭제 예약" 상태로 남는다.
     * 멈춰 있으면 제거된 것으로 본다 (다음 /installtunnelservice 가 삭제 완료를 기다린다). */
    if (svc_query(name, &st) && st == SERVICE_STOPPED) {
        warn("터널 서비스가 '삭제 예약' 상태입니다. 서비스 관리자/WireGuard 창을 닫으면 정리됩니다.");
        return TRUE;
    }
    return FALSE;
}

BOOL tunnel_install(const char *name, const char *conf)
{
    char cmd[2 * MAX_PATH + 64], out[4096];
    DWORD code = 1;
    (void)name;   /* 서비스 이름은 설정 파일 이름(<name>.conf)에서 정해진다 */
    snprintf(cmd, sizeof cmd, "\"%s\" /installtunnelservice \"%s\"", g_wgexe, conf);
    /* wireguard.exe 는 표준 오류가 있으면 오류 메시지를 거기에 쓴다(없으면 메시지 상자). 그대로 보여준다 */
    if (run(cmd, NULL, out, sizeof out, &code) && code == 0) return TRUE;
    trim(out);
    say(C_RED, "wireguard.exe /installtunnelservice 실패 (종료 코드 %lu)\n  %s\n", (unsigned long)code, *out ? out : "(출력 없음)");
    tunnel_diagnose(name);
    return FALSE;
}

static BOOL adapter_exists(const char *name)
{
    ULONG sz = 16384, r;
    IP_ADAPTER_ADDRESSES *a, *p;
    wchar_t wn[64];
    BOOL found = FALSE;
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wn, 64);
    for (int tries = 0; tries < 3; tries++) {
        a = (IP_ADAPTER_ADDRESSES *)malloc(sz);
        if (!a) return FALSE;
        r = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, a, &sz);
        if (r == ERROR_SUCCESS) {
            for (p = a; p; p = p->Next)
                if (p->FriendlyName && !_wcsicmp(p->FriendlyName, wn)) { found = TRUE; break; }
            free(a);
            return found;
        }
        free(a);
        if (r != ERROR_BUFFER_OVERFLOW) break;
    }
    return FALSE;
}


BOOL tunnel_wait_ready(const char *name, int timeout_ms)
{
    for (int waited = 0; waited <= timeout_ms; waited += 500) {
        if (adapter_exists(name)) return TRUE;
        Sleep(500);
    }
    return FALSE;
}

/* 터널이 안 뜰 때: 서비스 상태와 WireGuard 로그(wireguard.exe /dumplog)의 마지막 부분을 보여준다 */
void tunnel_diagnose(const char *name)
{
    static char log[65536];
    char cmd[MAX_PATH + 32];
    const char *st = tunnel_status(name);
    const char *p;
    int lines = 0;

    pf("  진단: 서비스 WireGuardTunnel$%s = %s\n", name, st ? st : "없음");
    if (!wg_installed()) return;
    snprintf(cmd, sizeof cmd, "\"%s\" /dumplog", g_wgexe);
    if (!run(cmd, NULL, log, sizeof log, NULL) || !*log) { pf("  (WireGuard 로그를 읽지 못했습니다)\n"); return; }
    /* 마지막 25줄만 */
    for (p = log + strlen(log); p > log; p--)
        if (*p == '\n' && ++lines > 25) { p++; break; }
    pf("  WireGuard 로그 (wireguard.exe /dumplog, 마지막 부분):\n%s\n", p);
}
