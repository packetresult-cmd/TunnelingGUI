/* posix/gateway.c
 * Linux 게이트웨이 구성: iptables NAT(MASQUERADE + 포트 DNAT), IP 포워딩(sysctl), 호스트 방화벽(ufw/firewalld).
 *
 * NAT 규칙은 서버 터널 설정(/etc/wireguard/tgui-gw.conf)의 PostUp/PostDown 에 넣는다.
 * 그러면 wg-quick@tgui-gw 가 켜질 때 규칙이 생기고 꺼질 때 사라져서, 재부팅 후에도 자동 복구되고
 * 제거할 때 규칙이 남지 않는다.
 */
#include "tgui.h"
#include "pkg.h"

#define SYSCTL_FILE "/etc/sysctl.d/99-tgui.conf"

/* ------------------------------------------------------------------ 외부 인터페이스 / 사전 점검 */

static BOOL ext_iface(char *out, size_t sz)
{
    char o[512];
    DWORD code = 1;
    const char *p;
    size_t n = 0;

    if (*O.extif) { snprintf(out, sz, "%s", O.extif); return TRUE; }
    if (!run("ip -4 route show default", NULL, o, sizeof o, &code) || code != 0) return FALSE;
    p = strstr(o, " dev ");
    if (!p) return FALSE;
    p += 5;
    while (p[n] && p[n] != ' ' && p[n] != '\n' && n + 1 < sz) { out[n] = p[n]; n++; }
    out[n] = 0;
    return n > 0;
}

BOOL gateway_preflight(BOOL install_missing)
{
    static const char *const NEED[] = { "iptables", "ip", "sysctl", "curl" };
    char ext[32];
    BOOL ok = TRUE;

    for (size_t i = 0; i < sizeof NEED / sizeof NEED[0]; i++) {
        if (have_cmd(NEED[i])) continue;
        if (install_missing) {
            if (!ensure_cmd(NEED[i])) ok = FALSE;
        } else {
            say(C_YELLOW, "  '%s' 명령이 없습니다. (vps-setup 이 자동 설치합니다)\n", NEED[i]);
        }
    }
    if (!ok) return FALSE;
    if (!have_cmd("ip") || !have_cmd("iptables")) return TRUE;   /* 점검 모드: 도구가 없어 이후 점검 생략 */

    if (!ext_iface(ext, sizeof ext)) {
        say(C_RED, "인터넷 쪽 네트워크 인터페이스를 찾지 못했습니다. -ext-iface <이름> 으로 지정하세요.\n");
        return FALSE;
    }
    pf("  외부 인터페이스: %s\n", ext);
    if (!run_ok("iptables -t nat -S >/dev/null 2>&1")) {
        say(C_RED, "iptables nat 테이블을 쓸 수 없습니다. 커널 netfilter/NAT 모듈이나 컨테이너 권한(NET_ADMIN)을 확인하세요.\n");
        return FALSE;
    }
    return TRUE;
}

/* ------------------------------------------------------------------ NAT 규칙 (터널 설정의 PostUp/PostDown) */

/* 포트 배열(오름차순)의 연속 구간마다 DNAT 규칙 하나: 단일 "--dport P → IP:P", 범위 "--dport A:B → IP" (포트 유지) */
static void emit_dnat(char *out, size_t sz, const char *key, const char *op, const char *ext, const char *proto, const int *a, int n)
{
    char l[512];
    for (int i = 0; i < n;) {
        int j = i;
        while (j + 1 < n && a[j + 1] == a[j] + 1) j++;
        if (i == j)
            snprintf(l, sizeof l, "%s = iptables -t nat %s PREROUTING -i %s -p %s --dport %d -j DNAT --to-destination %s:%d" NL,
                     key, op, ext, proto, a[i], O.clientip, a[i]);
        else
            snprintf(l, sizeof l, "%s = iptables -t nat %s PREROUTING -i %s -p %s --dport %d:%d -j DNAT --to-destination %s" NL,
                     key, op, ext, proto, a[i], a[j], O.clientip);
        strncat(out, l, sz - strlen(out) - 1);
        i = j + 1;
    }
}

static void emit_rules(char *out, size_t sz, const char *key, const char *op, const char *ext)
{
    char l[512];
    /* op: "-A"(추가) 또는 "-D"(삭제). FORWARD 는 앞에 끼워 넣는다(-I) */
    const char *fwd = !strcmp(op, "-A") ? "-I" : "-D";

    snprintf(l, sizeof l, "%s = iptables -t nat %s POSTROUTING -s %s -o %s -j MASQUERADE" NL, key, op, O.subnet, ext);
    strncat(out, l, sz - strlen(out) - 1);
    snprintf(l, sizeof l, "%s = iptables %s FORWARD -i %%i -j ACCEPT" NL, key, fwd);
    strncat(out, l, sz - strlen(out) - 1);
    snprintf(l, sizeof l, "%s = iptables %s FORWARD -o %%i -j ACCEPT" NL, key, fwd);
    strncat(out, l, sz - strlen(out) - 1);
    emit_dnat(out, sz, key, op, ext, "udp", O.udp, O.nudp);
    emit_dnat(out, sz, key, op, ext, "tcp", O.tcp, O.ntcp);
}

void gateway_conf_extra(char *out, size_t sz)
{
    char ext[32] = "eth0";
    out[0] = 0;
    ext_iface(ext, sizeof ext);
    strncat(out, "PostUp = sysctl -w net.ipv4.ip_forward=1" NL, sz - strlen(out) - 1);
    emit_rules(out, sz, "PostUp", "-A", ext);
    emit_rules(out, sz, "PostDown", "-D", ext);
}

BOOL gateway_apply(void)
{
    char out[512];
    DWORD code = 1;

    step("IP 포워딩 활성화");
    if (!write_file(SYSCTL_FILE, "net.ipv4.ip_forward = 1" NL)) {
        say(C_RED, "%s 를 쓸 수 없습니다.\n", SYSCTL_FILE);
        return FALSE;
    }
    chmod(SYSCTL_FILE, 0644);
    if (!run("sysctl -w net.ipv4.ip_forward=1", NULL, out, sizeof out, &code) || code != 0) {
        trim(out);
        say(C_RED, "sysctl 실패: %s\n", out);
        return FALSE;
    }
    pf("  net.ipv4.ip_forward = 1 (재부팅 후에도 유지: %s)\n", SYSCTL_FILE);

    step("NAT 규칙 (wg-quick 이 터널과 함께 적용)");
    if (!run("iptables -t nat -S POSTROUTING", NULL, out, sizeof out, &code) || !strstr(out, "MASQUERADE")) {
        say(C_RED, "NAT 규칙이 적용되지 않았습니다. 'wg-quick@" TUN_VPS "' 로그를 확인하세요.\n");
        return FALSE;
    }
    {
        char ps_[PORTS_STR_MAX];
        ports_str(O.udp, O.nudp, ps_, sizeof ps_);
        if (O.nudp) pf("  UDP %s → %s (같은 포트)\n", ps_, O.clientip);
        ports_str(O.tcp, O.ntcp, ps_, sizeof ps_);
        if (O.ntcp) pf("  TCP %s → %s (같은 포트)\n", ps_, O.clientip);
    }
    return TRUE;
}

BOOL gateway_present(void)
{
    char p[MAX_PATH], *t;
    BOOL has;
    join(p, sizeof p, g_wgdir, TUN_VPS ".conf");
    t = read_file(p);
    has = t && strstr(t, "MASQUERADE") != NULL;
    free(t);
    return has || file_exists(SYSCTL_FILE);
}

BOOL gateway_remove(void)
{
    pf("  NAT 규칙은 터널(wg-quick PostDown)과 함께 제거되었습니다.\n");
    return TRUE;
}

BOOL gateway_disable_routing(void)
{
    unlink(SYSCTL_FILE);
    return run_ok("sysctl -w net.ipv4.ip_forward=0");
}

void gateway_status(void)
{
    char out[2048];
    BOOL nat = run("iptables -t nat -S POSTROUTING", NULL, out, sizeof out, NULL) && strstr(out, O.subnet) != NULL;
    pf("NAT (%s) : %s\n", O.subnet, nat ? "있음" : "없음");
}

/* iptables PREROUTING 의 DNAT 규칙 중 게임 PC 로 가는 것만 보여준다 */
void gateway_list_ports(void)
{
    char out[16384], *line, *save = NULL;
    int shown = 0;
    if (run("iptables -t nat -S PREROUTING", NULL, out, sizeof out, NULL)) {
        for (line = strtok_r(out, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
            const char *p = strstr(line, " -p "), *d = strstr(line, "--dport "), *t = strstr(line, "--to-destination ");
            char proto[8] = "", dport[24] = "", to[64] = "";
            if (!p || !d || !t || !strstr(line, "DNAT")) continue;
            sscanf(p + 4, "%7s", proto);
            sscanf(d + 8, "%23s", dport);
            sscanf(t + 17, "%63s", to);
            if (strncmp(to, O.clientip, strlen(O.clientip))) continue;   /* 게임 PC 로 가는 규칙만 */
            for (char *c = proto; *c; c++) if (*c >= 'a' && *c <= 'z') *c = (char)(*c - 32);
            for (char *c = dport; *c; c++) if (*c == ':') *c = '-';
            pf("  %s %s -> %s\n", proto, dport, to);
            shown++;
        }
    }
    if (!shown) pf("  (없음)\n");
}

/* ------------------------------------------------------------------ 호스트 방화벽 (ufw / firewalld) */

typedef enum { FW_NONE, FW_UFW, FW_FIREWALLD } FwBackend;

static FwBackend fw_backend(void)
{
    char o[512];
    if (have_cmd("ufw") && run("LC_ALL=C ufw status", NULL, o, sizeof o, NULL) && strstr(o, "Status: active")) return FW_UFW;
    if (have_cmd("firewall-cmd") && run_ok("firewall-cmd --state >/dev/null 2>&1")) return FW_FIREWALLD;
    return FW_NONE;
}

const char *firewall_label(void)
{
    switch (fw_backend()) {
    case FW_UFW: return "방화벽 (ufw)";
    case FW_FIREWALLD: return "방화벽 (firewalld)";
    default: return "호스트 방화벽 (활성 ufw/firewalld 없음 - 변경 없음)";
    }
}

static void records_path(char *out, size_t sz) { join(out, sz, g_workdir, "firewall.rules"); }

/* 열었던 규칙을 "backend proto 포트(또는 A-B)" 줄로 기록해 두었다가 제거 때 그대로 되돌린다 */
static void record(const char *backend, const char *proto, const char *spec)
{
    char p[MAX_PATH], *old, *neu;
    size_t n;
    records_path(p, sizeof p);
    old = read_file(p);
    n = (old ? strlen(old) : 0) + 64;
    neu = (char *)malloc(n);
    if (!neu) { free(old); return; }
    snprintf(neu, n, "%s%s %s %s\n", old ? old : "", backend, proto, spec);
    write_file(p, neu);
    free(old);
    free(neu);
}

/* 포트 표기: ufw 는 범위를 "A:B", firewalld 는 "A-B" 로 쓴다 */
static void fw_cmd(char *cmd, size_t sz, FwBackend be, BOOL add, const char *proto, const char *spec)
{
    char s[24];
    snprintf(s, sizeof s, "%s", spec);
    if (be == FW_UFW) {
        for (char *c = s; *c; c++) if (*c == '-') *c = ':';
        snprintf(cmd, sz, "ufw %sallow %s/%s", add ? "" : "delete ", s, proto);
    } else {
        snprintf(cmd, sz, "firewall-cmd --permanent --%s-port=%s/%s", add ? "add" : "remove", s, proto);
    }
}

static BOOL valid_spec(const char *s)
{
    int lo, hi;
    char extra;
    if (sscanf(s, "%d-%d%c", &lo, &hi, &extra) == 2) return lo >= 1 && hi <= 65535 && lo <= hi;
    return sscanf(s, "%d%c", &lo, &extra) == 1 && lo >= 1 && lo <= 65535;
}

BOOL firewall_rule(const char *name, const char *proto, const char *ports)
{
    FwBackend be = fw_backend();
    char list[PORTS_STR_MAX], lp[8], cmd[256], out[512], *tok, *save = NULL;
    size_t i;
    DWORD code = 1;

    (void)name;
    if (be == FW_NONE) return TRUE;
    for (i = 0; proto[i] && i + 1 < sizeof lp; i++) lp[i] = (char)((proto[i] >= 'A' && proto[i] <= 'Z') ? proto[i] + 32 : proto[i]);
    lp[i] = 0;
    snprintf(list, sizeof list, "%s", ports);
    for (tok = strtok_r(list, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        if (!valid_spec(tok)) continue;
        fw_cmd(cmd, sizeof cmd, be, TRUE, lp, tok);
        if (!run(cmd, NULL, out, sizeof out, &code) || code != 0) {
            trim(out);
            say(C_RED, "방화벽 규칙 추가 실패: %s\n", out);
            return FALSE;
        }
        record(be == FW_UFW ? "ufw" : "firewalld", lp, tok);
    }
    if (be == FW_FIREWALLD) run_ok("firewall-cmd --reload");
    return TRUE;
}

BOOL firewall_remove(void)
{
    char p[MAX_PATH], *t, *line, *save = NULL, cmd[256], be[16], proto[8], spec[24];
    BOOL reload = FALSE;

    records_path(p, sizeof p);
    t = read_file(p);
    if (!t) return TRUE;
    for (line = strtok_r(t, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        if (sscanf(line, "%15s %7s %23s", be, proto, spec) != 3 || !valid_spec(spec)) continue;
        fw_cmd(cmd, sizeof cmd, !strcmp(be, "ufw") ? FW_UFW : FW_FIREWALLD, FALSE, proto, spec);
        if (strcmp(be, "ufw")) reload = TRUE;
        run_ok(cmd);
    }
    if (reload) run_ok("firewall-cmd --reload");
    free(t);
    unlink(p);
    return TRUE;
}

int fw_count(void)
{
    char p[MAX_PATH], *t, *c;
    int n = 0;
    records_path(p, sizeof p);
    t = read_file(p);
    if (!t) return 0;
    for (c = t; *c; c++) if (*c == '\n') n++;
    free(t);
    return n;
}

void firewall_list(void)
{
    char p[MAX_PATH], *t, *line, *save = NULL, be[16], proto[8], spec[24];
    int shown = 0;
    records_path(p, sizeof p);
    t = read_file(p);
    if (t) {
        for (line = strtok_r(t, "\n", &save); line; line = strtok_r(NULL, "\n", &save))
            if (sscanf(line, "%15s %7s %23s", be, proto, spec) == 3) { pf("  %s: %s %s\n", be, proto, spec); shown++; }
        free(t);
    }
    if (!shown) pf("  (없음%s)\n", fw_backend() == FW_NONE ? " - 활성 ufw/firewalld 없음" : "");
}

/* Linux 는 iptables 를 쓰므로 켤 기능이 없다 */
int gateway_enable_nat_feature(void)
{
    pf("Linux 에서는 필요 없습니다 (iptables NAT 사용).\n");
    return 0;
}
