/* config.c
 * 프로그램 옵션 기본값과 전역 경로 변수.
 * (경로 값은 플랫폼별 init_paths() 가 채운다)
 */
#include "tgui.h"

Opts O;
char g_workdir[MAX_PATH], g_wgdir[MAX_PATH], g_wg[MAX_PATH], g_wgexe[MAX_PATH];

void opts_default(void)
{
    memset(&O, 0, sizeof O);
    O.wgport = 51820;
    strcpy(O.subnet, "10.8.0.0/24");
    strcpy(O.serverip, "10.8.0.1");
    strcpy(O.clientip, "10.8.0.2");
    strcpy(O.dns, "1.1.1.1");
    O.mtu = 1420;
    O.udp[0] = 7777; O.udp[1] = 7778; O.udp[2] = 27015; O.nudp = 3;
    O.qport = 27015;
    O.timeout_ms = 3000;
}
