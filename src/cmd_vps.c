/* cmd_vps.c
 * 명령: vps-check / vps-setup / vps-status / vps-uninstall.
 * 플랫폼별 차이(Windows: WinNAT, Linux: iptables)는 gateway.h / tunnel.h 뒤에 숨겨져 있다.
 */
#include "tgui.h"

int cmd_vps_check(void)
{
    step("VPS 게이트웨이 사전 점검");
    if (wg_installed()) pf("  WireGuard: 설치됨\n"); else pf("  WireGuard: 없음 (vps-setup 이 자동으로 설치합니다)\n");
    if (!gateway_preflight(FALSE)) return 1;
    say(C_GREEN, "점검 통과: vps-setup 을 실행할 수 있습니다.\n");
    return 0;
}

int cmd_vps_setup(void)
{
    char keyfile[MAX_PATH], sconf[MAX_PATH], cconf[MAX_PATH], pub[64], *kj = NULL, text[8192], out[1024];
    /* 포트가 많으면(최대 256개 x 2) Linux PostUp/PostDown 규칙이 길어지므로 넉넉한 정적 버퍼 */
    static char extra[256 * 1024], server_text[sizeof extra + 4096];
    KeyPair srv, cli;
    char udps[PORTS_STR_MAX], tcps[PORTS_STR_MAX], prefix[8];
    BOOL reuse = FALSE;

    step("사전 확인");
    if (!ensure_wireguard()) return 1;
    if (!gateway_preflight(TRUE)) return 1;
    if (!ensure_workdir()) return 1;
    join(keyfile, sizeof keyfile, g_workdir, "keys.json");
    join(sconf, sizeof sconf, g_workdir, TUN_VPS ".conf");
    join(cconf, sizeof cconf, g_workdir, "client-gamepc.conf");
    snprintf(prefix, sizeof prefix, "%s", strchr(O.subnet, '/') + 1);
    ports_str(O.udp, O.nudp, udps, sizeof udps);
    ports_str(O.tcp, O.ntcp, tcps, sizeof tcps);

    step("키 준비");
    if (!O.regenerate && (kj = read_file(keyfile)) != NULL) {
        reuse = json_val(kj, "Server", "Private", srv.priv, sizeof srv.priv) &&
                json_val(kj, "Server", "Public", srv.pub, sizeof srv.pub) &&
                json_val(kj, "Client", "Private", cli.priv, sizeof cli.priv) &&
                json_val(kj, "Client", "Public", cli.pub, sizeof cli.pub);
        free(kj);
    }
    if (reuse) {
        pf("  기존 키 재사용\n");
    } else {
        if (!gen_keypair(&srv) || !gen_keypair(&cli)) { say(C_RED, "키 생성 실패 (wg)\n"); return 1; }
        snprintf(text, sizeof text,
                 "{" NL "  \"Server\": {" NL "    \"Private\": \"%s\"," NL "    \"Public\": \"%s\"" NL "  }," NL
                 "  \"Client\": {" NL "    \"Private\": \"%s\"," NL "    \"Public\": \"%s\"" NL "  }" NL "}" NL,
                 srv.priv, srv.pub, cli.priv, cli.pub);
        if (!write_file(keyfile, text)) { say(C_RED, "keys.json 저장 실패\n"); return 1; }
        pf("  새 키 생성 (게임 PC 설정도 새로 적용해야 합니다)\n");
    }

    step("공인 IP 확인");
    snprintf(pub, sizeof pub, "%s", O.publicip);
    if (!*pub)
        http_text("http://169.254.169.254/metadata/instance/network/interface/0/ipv4/ipAddress/0/publicIpAddress?api-version=2021-02-01&format=text",
                  "Metadata: true", pub, sizeof pub, 3000);
    if (!valid_ip(pub)) pub[0] = 0;
    if (!*pub && !egress_ip(pub, sizeof pub)) { say(C_RED, "공인 IP를 알아내지 못했습니다. -public-ip 로 직접 지정하세요.\n"); return 1; }
    if (!valid_ip(pub)) { say(C_RED, "공인 IP 형식이 올바르지 않습니다: %s\n", pub); return 1; }
    pf("  VPS 공인 IP: %s\n", pub);

    step("WireGuard 서버 터널 설치");
    gateway_conf_extra(extra, sizeof extra);
    snprintf(server_text, sizeof server_text,
             "[Interface]" NL "PrivateKey = %s" NL "ListenPort = %d" NL "Address = %s/%s" NL "%s" NL
             "[Peer]" NL "# 게임 PC" NL "PublicKey = %s" NL "AllowedIPs = %s/32" NL,
             srv.priv, O.wgport, O.serverip, prefix, extra, cli.pub, O.clientip);
    if (!write_file(sconf, server_text)) { say(C_RED, "서버 설정 저장 실패\n"); return 1; }
    if (!tunnel_remove(TUN_VPS)) { say(C_RED, "기존 터널을 제거하지 못했습니다.\n"); return 1; }
    if (!tunnel_install(TUN_VPS, sconf)) { say(C_RED, "터널 설치 실패\n"); return 1; }
    if (!tunnel_wait_ready(TUN_VPS, 30000)) {
        say(C_RED, "터널 인터페이스(" TUN_VPS ")가 30초 안에 올라오지 않았습니다.\n");
        tunnel_diagnose(TUN_VPS);
        return 1;
    }
    pf("  인터페이스 " TUN_VPS " 활성\n");

    if (!gateway_apply()) return 1;

    step("%s", firewall_label());
    if (!firewall_remove()) return 1;
    snprintf(out, sizeof out, "%d", O.wgport);
    if (!firewall_rule("TunnelingGUI - WireGuard", "UDP", out)) return 1;
    if (O.nudp && !firewall_rule("TunnelingGUI - Game UDP", "UDP", udps)) return 1;
    if (O.ntcp && !firewall_rule("TunnelingGUI - Game TCP", "TCP", tcps)) return 1;

    step("게임 PC용 설정 생성");
    /* 0.0.0.0/0 대신 /1 두 개: wireguard-windows 의 자동 킬스위치(LAN 차단)를 피하면서 전체 트래픽을 터널로 보냄 */
    snprintf(text, sizeof text,
             "[Interface]" NL "PrivateKey = %s" NL "Address = %s/%s" NL "DNS = %s" NL "MTU = %d" NL NL
             "[Peer]" NL "# TunnelingGUI gateway" NL "PublicKey = %s" NL "Endpoint = %s:%d" NL
             "AllowedIPs = 0.0.0.0/1, 128.0.0.0/1" NL "PersistentKeepalive = 25" NL,
             cli.priv, O.clientip, prefix, O.dns, O.mtu, srv.pub, pub, O.wgport);
    if (!write_file(cconf, text)) { say(C_RED, "클라이언트 설정 저장 실패\n"); return 1; }

    if (!settings_save("vps", pub)) warn("설정(settings.conf)을 저장하지 못했습니다.");

    pf("\n");
    say(C_GREEN, "완료.\n");
    pf("  열린 포트          : UDP %s%s%s\n", *udps ? udps : "(없음)", O.ntcp ? " / TCP " : "", O.ntcp ? tcps : "");
    pf("  게임 PC 설정 파일 : %s\n", cconf);
    pf("    → 개인키가 들어 있으니 안전한 방법(RDP 클립보드/드라이브, scp 등)으로 게임 PC에 옮기세요.\n\n");
    say(C_YELLOW, "남은 작업 (클라우드 방화벽: Azure NSG / AWS 보안 그룹 / 호스팅 패널의 인바운드 규칙):\n");
    pf("  UDP %d, %s 허용\n", O.wgport, udps);
    if (O.ntcp) pf("  TCP %s 허용\n", tcps);
    return 0;
}

/* ------------------------------------------------------------------ 상태 / 제거 */
int cmd_vps_status(void)
{
    char cmd[MAX_PATH + 64], out[2048];
    const char *st = tunnel_status(TUN_VPS);
    pf("터널 " TUN_VPS " : %s\n", st ? st : "없음");
    if (st && wg_installed()) {
        snprintf(cmd, sizeof cmd, "\"%s\" show " TUN_VPS, g_wg);
        if (run(cmd, NULL, out, sizeof out, NULL)) { trim(out); pf("%s\n", out); }
    }
    gateway_status();
    pf("포트 전달:\n");
    gateway_list_ports();
    pf("방화벽 규칙(%s) : %d개\n", firewall_label(), fw_count());
    return 0;
}

int cmd_vps_uninstall(void)
{
    const char *st = tunnel_status(TUN_VPS);
    BOOL svc = st != NULL, nat = gateway_present();
    int fw = fw_count();

    if (tunnel_exists(TUN_GAMEPC)) {
        warn("이 PC에는 게임 PC용 터널(" TUN_GAMEPC ")이 있습니다. 여기는 게임 PC 같습니다. '게임 PC 제거'를 쓰세요.");
        if (!svc) return 0;
    }
    if (!svc && !nat && !fw && !file_exists(g_workdir)) {
        say(C_YELLOW, "제거할 TunnelingGUI 터널 설정이 없습니다. (이미 제거됐거나 VPS가 아닙니다)\n");
        return 0;
    }
    pf("발견한 설정:\n  터널 " TUN_VPS "   : %s\n  NAT           : %s\n  방화벽 규칙   : %d개\n  설정 폴더     : %s\n\n",
       svc ? st : "없음", nat ? "있음" : "없음", fw, file_exists(g_workdir) ? g_workdir : "없음");
    if (!O.force) {
        say(C_YELLOW, "게임 PC에서 먼저 '터널 끄기/제거'를 실행하셨나요? (VPS를 먼저 지우면 게임 PC 인터넷이 끊깁니다)\n");
        if (!confirm("계속하려면 y 입력")) { pf("취소했습니다.\n"); return 0; }
    }

    step("터널 제거");
    if (svc) pf("  %s\n", tunnel_remove(TUN_VPS) ? "완료" : "실패: 터널이 남아 있습니다");
    else pf("  없음\n");

    step("NAT 규칙 제거");
    if (nat) { gateway_remove(); pf("  완료\n"); } else pf("  없음\n");

    step("방화벽 규칙 제거");
    if (fw) { firewall_remove(); pf("  완료\n"); } else pf("  없음\n");

    if (O.disable_routing) {
        step("IP 포워딩 끄기");
        gateway_disable_routing();
        pf("  완료 (Windows 는 재부팅 후 완전히 적용)\n");
    }
    if (O.purge) {
        step("키와 설정 파일 삭제");
        if (file_exists(g_workdir)) { rmtree(g_workdir); pf("  %s 삭제\n", g_workdir); } else pf("  없음\n");
        pf("  다시 설치하면 새 키가 만들어지므로 게임 PC에도 새 conf를 설치해야 합니다.\n");
    }
    pf("\n");
    say(C_GREEN, "VPS 쪽 제거 완료.\n");
    if (!O.purge) pf("  키와 설정은 %s 에 남아 있습니다. 다시 설치하면 같은 키를 재사용합니다.\n", g_workdir);
    pf("  클라우드/호스팅 쪽 인바운드 규칙(UDP %d, 게임 포트)은 직접 지우세요.\n", O.wgport);
    pf("  WireGuard 프로그램 자체는 지우지 않았습니다.\n");
    return 0;
}

/* ------------------------------------------------------------------ winnat-enable */
int cmd_winnat_enable(void)
{
    step("WinNAT 기능 켜기 (Windows)");
    pf("  New-NetNat 이 '잘못된 클래스입니다' 로 실패하는 Windows 에서 필요합니다.\n"
       "  Hyper-V(Home 은 HypervisorPlatform)를 켜고, 재부팅이 필요합니다.\n"
       "  VirtualBox/VMware 같은 다른 가상화 프로그램의 동작 방식이 바뀔 수 있습니다.\n");
    if (!O.force && !confirm("계속하려면 y 입력")) { pf("취소했습니다.\n"); return 0; }
    return gateway_enable_nat_feature();
}
