/* menu.c
 * 인자 없이 실행했을 때의 대화형 메뉴.
 */
#include "tgui.h"

static void menu_ports(const char *label, int *arr, int *n)
{
    char def[PORTS_STR_MAX], v[PORTS_STR_MAX];
    ports_str(arr, *n, def, sizeof def);
    for (;;) {
        ask(label, def, v, sizeof v);
        if (!strcmp(v, "-")) { *n = 0; return; }
        if (parse_ports(v, arr, n)) return;
        say(C_RED, "포트 목록 형식이 올바르지 않습니다. (예: 7777,7778)\n");
    }
}

int menu(void)
{
    for (;;) {
        char c[16], v[256];
        int rc = 0;
        pf("\n");
        say(C_CYAN, "=== TunnelingGUI %s ===\n", APP_VERSION);
        pf(" 1) WireGuard 설치/확인\n"
           " [VPS]\n"
           " c) 사전 점검 (vps-check)\n"
           " 2) 게이트웨이 구성 (vps-setup)      3) 상태      4) 제거\n"
           " [게임 PC]\n"
           " 5) 설정 설치   6) 터널 켜기   7) 터널 끄기   8) 상태   9) 제거\n"
           " [공통]\n"
           " p) 열 포트 보기/변경\n"
           " v) 검증 (A2S / Steam 목록)\n"
           " 0) 종료\n");
        ask("선택", NULL, c, sizeof c);
        switch (c[0]) {
        case '1': rc = cmd_install_wireguard(); break;
        case 'c': case 'C': rc = cmd_vps_check(); break;
        case '2':
            ask("VPS 공인 IP (비우면 자동 감지)", "", O.publicip, sizeof O.publicip);
            menu_ports("게임 UDP 포트 ('-' = 없음)", O.udp, &O.nudp);
            menu_ports("게임 TCP 포트 ('-' = 없음)", O.tcp, &O.ntcp);
            ask("키 재생성? (y/N)", "n", v, sizeof v);
            O.regenerate = (v[0] == 'y' || v[0] == 'Y');
            rc = cmd_vps_setup();
            break;
        case '3': rc = cmd_vps_status(); break;
        case '4':
            ask("키/설정 폴더도 삭제? (y/N)", "n", v, sizeof v); O.purge = (v[0] == 'y' || v[0] == 'Y');
            ask("IP 포워딩도 끄기? (y/N)", "n", v, sizeof v); O.disable_routing = (v[0] == 'y' || v[0] == 'Y');
            rc = cmd_vps_uninstall();
            break;
        case '5':
            ask("client-gamepc.conf 경로", O.config, O.config, sizeof O.config);
            if (*O.config == '"') { memmove(O.config, O.config + 1, strlen(O.config)); if (strlen(O.config) && O.config[strlen(O.config) - 1] == '"') O.config[strlen(O.config) - 1] = 0; }
            menu_ports("ARK UDP 포트 ('-' = 없음)", O.udp, &O.nudp);
            menu_ports("ARK TCP 포트 ('-' = 없음)", O.tcp, &O.ntcp);
            rc = cmd_gamepc_install();
            break;
        case '6': rc = cmd_gamepc_up(); break;
        case '7': rc = cmd_gamepc_down(); break;
        case '8': rc = cmd_gamepc_status(); break;
        case '9':
            ask("설정 폴더(개인키)는 유지? (y/N)", "n", v, sizeof v); O.keep_config = (v[0] == 'y' || v[0] == 'Y');
            rc = cmd_gamepc_uninstall();
            break;
        case 'p': case 'P':
            cmd_ports();
            ask("포트를 바꿀까요? (y/N)", "n", v, sizeof v);
            if (v[0] == 'y' || v[0] == 'Y') {
                menu_ports("UDP 포트 ('-' = 없음)", O.udp, &O.nudp);
                menu_ports("TCP 포트 ('-' = 없음)", O.tcp, &O.ntcp);
                O.ports_given = 1;
                rc = cmd_ports();
            }
            break;
        case 'v': case 'V':
            ask("확인할 주소 (VPS 에서는 10.8.0.2, 외부에서는 VPS 공인 IP)", O.target, O.target, sizeof O.target);
            ask("쿼리 포트", "27015", v, sizeof v); O.qport = atoi(v);
            ask("Steam API 키 (생략 가능)", "", O.steamkey, sizeof O.steamkey);
            rc = cmd_verify();
            break;
        case '0': case 'q': case 'Q': return 0;
        default: say(C_YELLOW, "잘못된 선택입니다.\n"); continue;
        }
        if (rc) say(C_YELLOW, "(종료 코드 %d)\n", rc);
        O.ports_given = 0; O.force = 0; O.purge = 0; O.disable_routing = 0; O.keep_config = 0; O.regenerate = 0;
    }
}
