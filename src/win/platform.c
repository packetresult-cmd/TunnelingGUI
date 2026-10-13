/* win/platform.c
 * Windows 경로 초기화.
 */
#include "tgui.h"

void init_paths(void)
{
    char pd[MAX_PATH] = "C:\\ProgramData", pf64[MAX_PATH] = "";
    GetEnvironmentVariableA("ProgramData", pd, sizeof pd);
    if (!GetEnvironmentVariableA("ProgramW6432", pf64, sizeof pf64))
        if (!GetEnvironmentVariableA("ProgramFiles", pf64, sizeof pf64)) strcpy(pf64, "C:\\Program Files");
    join(g_workdir, sizeof g_workdir, pd, "TunnelingGUI");
    join(g_wgdir, sizeof g_wgdir, pf64, "WireGuard");
    join(g_wg, sizeof g_wg, g_wgdir, "wg.exe");
    join(g_wgexe, sizeof g_wgexe, g_wgdir, "wireguard.exe");
}
