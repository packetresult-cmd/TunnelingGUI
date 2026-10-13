/* posix/platform.c
 * Linux 경로 초기화.
 */
#include "tgui.h"

void init_paths(void)
{
    snprintf(g_workdir, sizeof g_workdir, "/etc/tgui");
    snprintf(g_wgdir, sizeof g_wgdir, "/etc/wireguard");
    snprintf(g_wg, sizeof g_wg, "wg");
    snprintf(g_wgexe, sizeof g_wgexe, "wg-quick");
}
