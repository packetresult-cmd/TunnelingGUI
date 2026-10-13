/* commands.h - 사용자 명령 */
#ifndef TGUI_COMMANDS_H
#define TGUI_COMMANDS_H

int cmd_install_wireguard(void);
int cmd_vps_setup(void);
int cmd_vps_check(void);
int cmd_winnat_enable(void);
int cmd_vps_status(void);
int cmd_ports(void);
int cmd_vps_uninstall(void);
int cmd_gamepc_install(void);
int cmd_gamepc_up(void);
int cmd_gamepc_down(void);
int cmd_gamepc_status(void);
int cmd_gamepc_uninstall(void);
int cmd_verify(void);

#endif
