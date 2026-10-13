/* cmd_ports.c
 * 명령: ports — 어떤 UDP/TCP 포트를 열지 보고 바꾸기.
 *   tgui ports                         저장된 설정 + 실제 적용 상태(NAT 전달, 방화벽) 보기
 *   tgui ports -udp 7777,27015 -tcp -  포트 변경을 바로 적용 (VPS: vps-setup 재실행, 게임 PC: 방화벽만)
 *
 * ARK 기본: 7777 게임(UDP), 7778 Raw UDP(UDP), 27015 서버 목록 쿼리(UDP), 27020 RCON(TCP, 선택)
 */
#include "tgui.h"

static void show_configured(void)
{
    char udps[PORTS_STR_MAX], tcps[PORTS_STR_MAX], role[16] = "", p[MAX_PATH];
    ports_str(O.udp, O.nudp, udps, sizeof udps);
    ports_str(O.tcp, O.ntcp, tcps, sizeof tcps);
    settings_path(p, sizeof p);
    step("설정된 포트");
    if (settings_role(role, sizeof role)) pf("  설정 파일 : %s (역할: %s)\n", p, role);
    else pf("  설정 파일 : 없음 (아직 vps-setup / gamepc-install 을 하지 않았습니다. 아래는 기본값)\n");
    pf("  UDP       : %s\n", *udps ? udps : "(없음)");
    pf("  TCP       : %s\n", *tcps ? tcps : "(없음)");
    pf("  WireGuard : UDP %d (VPS 쪽에서만 엽니다)\n", O.wgport);
}

int cmd_ports(void)
{
    char gconf[MAX_PATH];
    BOOL vps, gamepc;
    join(gconf, sizeof gconf, g_workdir, TUN_GAMEPC ".conf");
    vps = tunnel_exists(TUN_VPS);                                  /* VPS 구성이 설치돼 있음 */
    gamepc = !vps && (tunnel_exists(TUN_GAMEPC) || file_exists(gconf));   /* 게임 PC 설정이 설치돼 있음 */

    if (O.ports_given) {
        char ports[PORTS_STR_MAX];
        if (vps) {
            pf("VPS 의 열 포트를 바꿉니다. (vps-setup 을 같은 키로 다시 적용)\n");
            return cmd_vps_setup();
        }
        if (gamepc) {
            step("게임 PC 방화벽 포트 변경 (%s)", firewall_label());
            if (!firewall_remove()) return 1;
            if (O.nudp) { ports_str(O.udp, O.nudp, ports, sizeof ports); if (!firewall_rule("TunnelingGUI - ARK UDP", "UDP", ports)) return 1; }
            if (O.ntcp) { ports_str(O.tcp, O.ntcp, ports, sizeof ports); if (!firewall_rule("TunnelingGUI - ARK TCP", "TCP", ports)) return 1; }
            settings_save("gamepc", NULL);
            say(C_GREEN, "완료. 포트 변경은 VPS 쪽에도 같은 값으로 적용해야 합니다(VPS 에서 ports -udp ...).\n");
            return 0;
        }
        say(C_RED, "아직 설치된 구성이 없습니다. 먼저 vps-setup 또는 gamepc-install 을 실행하세요 (같은 -udp/-tcp 옵션 사용).\n");
        return 1;
    }

    show_configured();
    if (vps) {
        step("실제 적용된 포트 전달 (VPS 공인 IP → %s)", O.clientip);
        gateway_list_ports();
    }
    step("방화벽 규칙 (%s)", firewall_label());
    firewall_list();
    pf("\n바꾸려면: ports -udp 7777,7778,27015 -tcp 27020   ('-' = 없음)\n");
    pf("ARK 기본: 7777 게임 · 7778 Raw UDP · 27015 서버 목록 쿼리 (모두 UDP) · 27020 RCON (TCP, 선택)\n");
    return 0;
}
