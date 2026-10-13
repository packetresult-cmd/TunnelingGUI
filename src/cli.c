/* cli.c
 * 명령행 옵션 파싱, 검증, 도움말, 명령 분배.
 */
#include "tgui.h"

void usage(void)
{
#ifdef _WIN32
#  define EXE_NAME   "tgui.exe"
#  define PRIV_NOTE  "관리자 권한 필요"
#  define WG_NOTE    "공식 MSI + 서명 검증"
#else
#  define EXE_NAME   "tgui"
#  define PRIV_NOTE  "root 권한 필요, sudo 자동 사용"
#  define WG_NOTE    "배포판 패키지 관리자"
#endif
    pf("TunnelingGUI " APP_VERSION " — WireGuard 전체 터널(Type C) 설치/관리 (" PRIV_NOTE ")\n\n"
       "사용법: " EXE_NAME " <명령> [옵션]      (인자 없이 실행하면 메뉴)\n\n"
       "명령:\n"
       "  install-wireguard    WireGuard 설치(없을 때만, " WG_NOTE ")\n"
       "  vps-check            [VPS] 설치 전 점검 (NAT/WMI/iptables 사용 가능 여부, 변경 없음)\n"
       "  vps-setup            [VPS] 게이트웨이 구성 + client-gamepc.conf 생성\n"
       "  winnat-enable        [VPS, Windows] New-NetNat 이 '잘못된 클래스' 로 실패할 때 Hyper-V 등 기능 켜기 (재부팅 필요)\n"
       "  vps-status           [VPS] 터널/NAT/방화벽 상태\n"
       "  ports                열 포트 보기 (저장된 설정 + 실제 적용된 NAT/방화벽). -udp/-tcp 를 주면 바로 변경 적용\n"
       "  vps-uninstall        [VPS] 구성 제거\n"
       "  gamepc-install       [게임 PC] 설정 설치 + 방화벽 (-config 필요)\n"
       "  gamepc-up            [게임 PC] 터널 켜기 + 출구 IP 확인\n"
       "  gamepc-down          [게임 PC] 터널 끄기\n"
       "  gamepc-status        [게임 PC] 상태\n"
       "  gamepc-uninstall     [게임 PC] 설정 제거\n"
       "  verify               A2S_INFO 쿼리 / Steam 마스터 목록 확인 (-target 필요)\n\n"
       "옵션 (-name value 또는 --name=value):\n"
       "  -config <경로>        gamepc-install: client-gamepc.conf 경로\n"
       "  -public-ip <IP>       vps-setup: VPS 공인 IP (생략 시 자동 감지)\n"
       "  -wg-port <N>          WireGuard 포트 (기본 51820)\n"
       "  -udp <목록>           열 게임 UDP 포트. 개수 제한 없음(최대 256), 범위 가능: 7777-7790,27015-27020\n"
       "                        (기본 7777,7778,27015 / '-' = 없음). 한 번 정하면 저장되어 다음에도 유지\n"
       "  -tcp <목록>           열 게임 TCP 포트 (기본 없음, 예: RCON 27020, 범위 가능)\n"
       "  -subnet <CIDR>  -server-ip <IP>  -client-ip <IP>  -dns <IP>  -mtu <N>\n"
       "  -regenerate           vps-setup: 키 재생성\n"
       "  -purge                vps-uninstall: 키/설정 폴더 삭제\n"
       "  -disable-routing      vps-uninstall: IP 포워딩 끄기\n"
       "  -keep-config          gamepc-uninstall: 설정 폴더 유지\n"
       "  -force                확인 질문 생략\n"
       "  -target <주소>  -query-port <N>  -steam-key <KEY>  -timeout <ms>   verify 옵션\n"
       "  -wg-version <X.Y.Z>   설치할 WireGuard 버전 지정 (기본: 최신)\n"
       "  -no-auto-install      WireGuard 자동 설치 끄기\n"
       "  -ext-iface <이름>     vps-setup(Linux): 인터넷 쪽 네트워크 인터페이스 (기본: 기본 경로에서 자동 감지)\n");
}

static BOOL take(int argc, char **argv, int *i, const char *name, char *dst, size_t sz)
{
    const char *a = argv[*i];
    size_t n = strlen(name);
    while (*a == '-') a++;
    if (_strnicmp(a, name, n)) return FALSE;
    if (a[n] == '=') { snprintf(dst, sz, "%s", a + n + 1); return TRUE; }
    if (a[n]) return FALSE;
    if (*i + 1 >= argc) return FALSE;
    snprintf(dst, sz, "%s", argv[++*i]);
    return TRUE;
}

BOOL flag(const char *a, const char *name)
{
    while (*a == '-') a++;
    return !_stricmp(a, name);
}

/* "-" 또는 "none" 은 빈 목록(포트 없음) */
static BOOL parse_ports_opt(const char *v, int *arr, int *n)
{
    if (!strcmp(v, "-") || !_stricmp(v, "none")) { *n = 0; return TRUE; }
    return parse_ports(v, arr, n);
}

BOOL parse_opts(int argc, char **argv, int from)
{
    char v[PORTS_STR_MAX];
    for (int i = from; i < argc; i++) {
        if (flag(argv[i], "regenerate")) O.regenerate = 1;
        else if (flag(argv[i], "purge")) O.purge = 1;
        else if (flag(argv[i], "disable-routing")) O.disable_routing = 1;
        else if (flag(argv[i], "keep-config")) O.keep_config = 1;
        else if (flag(argv[i], "force") || flag(argv[i], "yes")) O.force = 1;
        else if (flag(argv[i], "no-auto-install")) O.no_auto_install = 1;
        else if (flag(argv[i], "elevated")) { /* 내부용 */ }
        else if (take(argc, argv, &i, "config", O.config, sizeof O.config)) {}
        else if (take(argc, argv, &i, "public-ip", O.publicip, sizeof O.publicip)) {}
        else if (take(argc, argv, &i, "target", O.target, sizeof O.target)) {}
        else if (take(argc, argv, &i, "steam-key", O.steamkey, sizeof O.steamkey)) {}
        else if (take(argc, argv, &i, "wg-version", O.wgver, sizeof O.wgver)) {}
        else if (take(argc, argv, &i, "ext-iface", O.extif, sizeof O.extif)) {}
        else if (take(argc, argv, &i, "wg-port", v, sizeof v)) { O.wgport = atoi(v); }
        else if (take(argc, argv, &i, "query-port", v, sizeof v)) { O.qport = atoi(v); }
        else if (take(argc, argv, &i, "timeout", v, sizeof v)) { O.timeout_ms = atoi(v); }
        else if (take(argc, argv, &i, "mtu", v, sizeof v)) { O.mtu = atoi(v); }
        else if (take(argc, argv, &i, "udp", v, sizeof v)) { if (!parse_ports_opt(v, O.udp, &O.nudp)) { say(C_RED, "잘못된 -udp 값: %s\n", v); return FALSE; } O.ports_given = 1; }
        else if (take(argc, argv, &i, "tcp", v, sizeof v)) { if (!parse_ports_opt(v, O.tcp, &O.ntcp)) { say(C_RED, "잘못된 -tcp 값: %s\n", v); return FALSE; } O.ports_given = 1; }
        else if (take(argc, argv, &i, "subnet", O.subnet, sizeof O.subnet)) {}
        else if (take(argc, argv, &i, "server-ip", O.serverip, sizeof O.serverip)) {}
        else if (take(argc, argv, &i, "client-ip", O.clientip, sizeof O.clientip)) {}
        else if (take(argc, argv, &i, "dns", O.dns, sizeof O.dns)) {}
        else { say(C_RED, "알 수 없는 옵션: %s\n", argv[i]); return FALSE; }
    }
    return TRUE;
}

BOOL validate_opts(void)
{
    if (O.wgport < 1 || O.wgport > 65535 || O.qport < 1 || O.qport > 65535) { say(C_RED, "포트 범위가 올바르지 않습니다.\n"); return FALSE; }
    if (O.mtu < 576 || O.mtu > 1500) { say(C_RED, "MTU 는 576~1500 이어야 합니다.\n"); return FALSE; }
    if (O.timeout_ms < 100) O.timeout_ms = 3000;
    if (!valid_subnet(O.subnet) || !valid_ip(O.serverip) || !valid_ip(O.clientip)) { say(C_RED, "-subnet/-server-ip/-client-ip 형식이 올바르지 않습니다.\n"); return FALSE; }
    if (!valid_dns(O.dns)) { say(C_RED, "-dns 형식이 올바르지 않습니다.\n"); return FALSE; }
    if (*O.wgver) for (const char *p = O.wgver; *p; p++) if (!((*p >= '0' && *p <= '9') || *p == '.')) { say(C_RED, "-wg-version 형식이 올바르지 않습니다.\n"); return FALSE; }
    for (const char *p = O.extif; *p; p++)
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_' || *p == '.' || *p == '-')) {
            say(C_RED, "-ext-iface 형식이 올바르지 않습니다.\n");
            return FALSE;
        }
    return TRUE;
}

int dispatch(const char *cmd)
{
    if (!_stricmp(cmd, "install-wireguard")) return cmd_install_wireguard();
    if (!_stricmp(cmd, "vps-check")) return cmd_vps_check();
    if (!_stricmp(cmd, "winnat-enable")) return cmd_winnat_enable();
    if (!_stricmp(cmd, "vps-setup")) return cmd_vps_setup();
    if (!_stricmp(cmd, "vps-status")) return cmd_vps_status();
    if (!_stricmp(cmd, "ports")) return cmd_ports();
    if (!_stricmp(cmd, "vps-uninstall")) return cmd_vps_uninstall();
    if (!_stricmp(cmd, "gamepc-install")) return cmd_gamepc_install();
    if (!_stricmp(cmd, "gamepc-up")) return cmd_gamepc_up();
    if (!_stricmp(cmd, "gamepc-down")) return cmd_gamepc_down();
    if (!_stricmp(cmd, "gamepc-status")) return cmd_gamepc_status();
    if (!_stricmp(cmd, "gamepc-uninstall")) return cmd_gamepc_uninstall();
    if (!_stricmp(cmd, "verify")) return cmd_verify();
    say(C_RED, "알 수 없는 명령: %s\n\n", cmd);
    usage();
    return 2;
}
