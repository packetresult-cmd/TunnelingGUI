/* cmd_wireguard.c
 * 명령: install-wireguard.
 */
#include "tgui.h"

int cmd_install_wireguard(void)
{
    if (wg_installed()) {
        say(C_GREEN, "WireGuard 는 이미 설치되어 있습니다: %s\n", g_wgdir);
        return 0;
    }
    return install_wireguard() ? 0 : 1;
}
