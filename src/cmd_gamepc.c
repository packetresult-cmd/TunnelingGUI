/* cmd_gamepc.c
 * 명령: gamepc-install / up / down / status / uninstall.
 */
#include "tgui.h"

static char *gamepc_conf_path(char *buf, size_t sz) { join(buf, sz, g_workdir, TUN_GAMEPC ".conf"); return buf; }

static BOOL gamepc_endpoint(char *host, size_t sz)
{
    char path[MAX_PATH], ep[128], *t, *c;
    BOOL ok = FALSE;
    t = read_file(gamepc_conf_path(path, sizeof path));
    if (!t) return FALSE;
    if (conf_get(t, "Endpoint", ep, sizeof ep)) {
        c = strrchr(ep, ':');
        if (c) *c = 0;
        snprintf(host, sz, "%s", ep);
        ok = TRUE;
    }
    free(t);
    return ok;
}

int cmd_gamepc_install(void)
{
    char dst[MAX_PATH], ports[PORTS_STR_MAX], *t, *adapted;

    if (!*O.config || !file_exists(O.config)) { say(C_RED, "-config 로 VPS에서 만든 client-gamepc.conf 경로를 지정하세요.\n"); return 1; }
    t = read_file(O.config);
    if (!t || !strstr(t, "[Interface]") || !strstr(t, "PrivateKey") || !strstr(t, "Endpoint")) {
        free(t);
        say(C_RED, "올바른 WireGuard 클라이언트 설정 파일이 아닙니다: %s\n", O.config);
        return 1;
    }
    if (!ensure_wireguard()) { free(t); return 1; }

    step("설정 파일 설치");
    if (!ensure_workdir()) { free(t); return 1; }
    gamepc_conf_path(dst, sizeof dst);
    adapted = adapt_client_conf(t);     /* 줄바꿈/DNS 등을 이 OS 에 맞게 보정 */
    free(t);
    if (!adapted || !write_file(dst, adapted)) { free(adapted); say(C_RED, "설정 파일 저장 실패: %s\n", dst); return 1; }
    free(adapted);
    pf("  %s\n", dst);

    step("방화벽 규칙 (%s)", firewall_label());
    if (!firewall_remove()) return 1;
    if (O.nudp) { ports_str(O.udp, O.nudp, ports, sizeof ports); if (!firewall_rule("TunnelingGUI - ARK UDP", "UDP", ports)) return 1; }
    if (O.ntcp) { ports_str(O.tcp, O.ntcp, ports, sizeof ports); if (!firewall_rule("TunnelingGUI - ARK TCP", "TCP", ports)) return 1; }
    if (!settings_save("gamepc", NULL)) warn("설정(settings.conf)을 저장하지 못했습니다.");
    say(C_GREEN, "완료. 다음: 터널 켜기 (gamepc-up)\n");
    return 0;
}

int cmd_gamepc_up(void)
{
    char conf[MAX_PATH], vps[128] = "", home[64] = "", egress[64] = "";
    long age = -1;
    int i;

    gamepc_conf_path(conf, sizeof conf);
    if (!file_exists(conf)) { say(C_RED, "먼저 설정 설치(gamepc-install)를 실행하세요.\n"); return 1; }
    if (!ensure_wireguard()) return 1;
    gamepc_endpoint(vps, sizeof vps);
    egress_ip(home, sizeof home);

    step("터널 켜기");
    if (tunnel_exists(TUN_GAMEPC)) { tunnel_remove(TUN_GAMEPC); Sleep(1000); }
    if (!tunnel_install(TUN_GAMEPC, conf)) { say(C_RED, "터널 설치 실패\n"); return 1; }
    if (!tunnel_wait_ready(TUN_GAMEPC, 30000)) {
        say(C_RED, "터널 인터페이스(" TUN_GAMEPC ")가 30초 안에 올라오지 않았습니다.\n");
        tunnel_diagnose(TUN_GAMEPC);
        return 1;
    }

    step("핸드셰이크 대기");
    for (i = 0; i < 30 && age < 0; i++) { Sleep(1000); age = handshake_age(TUN_GAMEPC); }
    if (age < 0) {
        warn("30초 동안 핸드셰이크가 없습니다. 클라우드 방화벽(Azure NSG 등)의 UDP 51820, VPS 설정 여부를 확인하세요.");
        tunnel_diagnose(TUN_GAMEPC);
        warn("인터넷이 끊긴 상태일 수 있으니 필요하면 터널 끄기(gamepc-down)로 복구하세요.");
        return 1;
    }
    pf("  핸드셰이크 OK (%ld초 전)\n", age);

    step("출구 IP 확인");
    for (i = 0; i < 5 && !egress_ip(egress, sizeof egress); i++) Sleep(1000);
    pf("  터널 전: %s\n  터널 후: %s\n  VPS    : %s\n", home, egress, vps);
    if (*egress && !strcmp(egress, vps)) {
        say(C_GREEN, "성공: 이 PC의 트래픽이 VPS IP로 나갑니다. 이제 ARK 서버를 시작하세요.\n");
        return 0;
    }
    warn("출구 IP가 VPS IP와 다릅니다. Azure 공인 IP가 바뀌었거나 라우팅 문제일 수 있습니다. 서버를 시작하지 마세요.");
    return 1;
}

int cmd_gamepc_down(void)
{
    step("터널 끄기");
    if (!tunnel_exists(TUN_GAMEPC)) { pf("이미 꺼져 있습니다.\n"); return 0; }
    if (!tunnel_remove(TUN_GAMEPC)) { say(C_RED, "터널을 제거하지 못했습니다.\n"); return 1; }
    say(C_GREEN, "완료. 집 회선으로 복귀했습니다.\n");
    return 0;
}

int cmd_gamepc_status(void)
{
    char conf[MAX_PATH], vps[128], eg[64];
    const char *st = tunnel_status(TUN_GAMEPC);
    BOOL have = st != NULL;
    long age;

    pf("터널       : %s\n", have ? st : "없음(꺼짐)");
    if (have && wg_installed()) {
        age = handshake_age(TUN_GAMEPC);
        if (age < 0) pf("핸드셰이크 : 없음\n"); else pf("핸드셰이크 : %ld초 전\n", age);
        if (age > 180) warn("3분 넘게 핸드셰이크가 없습니다. 연결이 끊겼을 수 있습니다.");
    }
    gamepc_conf_path(conf, sizeof conf);
    if (file_exists(conf) && gamepc_endpoint(vps, sizeof vps)) pf("VPS        : %s\n", vps);
    egress_ip(eg, sizeof eg);
    pf("현재 출구 IP: %s\n", eg);
    return 0;
}

int cmd_gamepc_uninstall(void)
{
    char eg[64];
    const char *st = tunnel_status(TUN_GAMEPC);
    BOOL svc = st != NULL;
    int fw = fw_count();

    if (tunnel_exists(TUN_VPS)) {
        warn("이 PC에는 VPS용 터널(" TUN_VPS ")이 있습니다. 여기는 VPS 같습니다. 'VPS 제거'를 쓰세요.");
        if (!svc) return 0;
    }
    if (!svc && !fw && !file_exists(g_workdir)) {
        say(C_YELLOW, "제거할 TunnelingGUI 터널 설정이 없습니다. (이미 제거됐거나 게임 PC가 아닙니다)\n");
        return 0;
    }
    pf("발견한 설정:\n  터널 " TUN_GAMEPC " : %s\n  방화벽 규칙 : %d개\n  설정 폴더   : %s%s\n\n",
       svc ? st : "없음", fw, file_exists(g_workdir) ? g_workdir : "없음", O.keep_config ? " (유지)" : "");
    if (!O.force) {
        say(C_YELLOW, "ARK 서버를 먼저 종료하셨나요? 제거하면 서버가 Steam 목록에서 빠집니다.\n");
        if (!confirm("계속하려면 y 입력")) { pf("취소했습니다.\n"); return 0; }
    }

    step("터널 제거 (집 회선으로 복귀)");
    if (svc) pf("  %s\n", tunnel_remove(TUN_GAMEPC) ? "완료" : "실패: 터널이 남아 있습니다");
    else pf("  없음\n");

    step("방화벽 규칙 제거");
    if (fw) { firewall_remove(); pf("  완료\n"); } else pf("  없음\n");

    if (!O.keep_config) {
        step("설정 폴더 삭제 (개인키 포함)");
        if (file_exists(g_workdir)) { rmtree(g_workdir); pf("  %s 삭제\n", g_workdir); } else pf("  없음\n");
    }

    step("인터넷 복귀 확인");
    if (egress_ip(eg, sizeof eg)) pf("  현재 출구 IP: %s (집 회선 IP여야 정상)\n", eg);
    else warn("  인터넷 확인 실패. 잠시 뒤 다시 확인하거나 재부팅해 보세요.");

    pf("\n");
    say(C_GREEN, "게임 PC 쪽 제거 완료.\n");
    pf("  남은 일:\n   1. ARK 실행 인자의 MultiHome=%s 를 원래 값으로 되돌리기 (안 하면 서버가 안 뜸)\n"
       "   2. VPS 에서 'VPS 제거' 실행\n"
       "   3. (선택) 다른 PC에 남은 client-gamepc.conf 복사본 삭제\n", O.clientip);
    return 0;
}
