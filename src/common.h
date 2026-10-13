/* common.h - 공용 상수, 옵션 구조체, 플랫폼 호환 정의 (Windows / POSIX) */
#ifndef TGUI_COMMON_H
#define TGUI_COMMON_H

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0601
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#  define PATH_SEP "\\"
#  define NL       "\r\n"            /* 설정 파일 줄바꿈 */
#  ifdef _MSC_VER
#    define strtok_r strtok_s           /* MSVC 에는 strtok_r 이 없다 (같은 시그니처의 strtok_s) */
#  endif
#else
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif
#  include <unistd.h>
#  include <errno.h>
#  include <strings.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <sys/socket.h>
#  include <netdb.h>
#  include <arpa/inet.h>
typedef int BOOL;
typedef unsigned long DWORD;
#  define TRUE  1
#  define FALSE 0
#  define MAX_PATH 1024
#  define PATH_SEP "/"
#  define NL       "\n"              /* wg-quick 은 CR 이 있으면 오동작한다 */
#  define _stricmp   strcasecmp
#  define _strnicmp  strncasecmp
#  define _strdup    strdup
#  define _strtoi64  strtoll
#  define Sleep(ms)  usleep((useconds_t)(ms) * 1000u)
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define APP_VERSION      "0.1.0"
#define DEFAULT_WG_VER   "0.5.3"
#define WG_BASE_URL      "https://download.wireguard.com/windows-client/"
#define TUN_GAMEPC       "tgui"          /* 게임 PC 쪽 터널 이름 */
#define TUN_VPS          "tgui-gw"       /* VPS 쪽 터널 이름 */
#define NAT_NAME         "TunnelingGUI"   /* WinNAT 이름 (Windows) */
#define FW_GROUP         "TunnelingGUI"   /* 방화벽 규칙 그룹 */
#define MAX_PORTS        256             /* 프로토콜별 최대 포트 수 (범위 7777-7790 포함) */
#define PORTS_STR_MAX    2048            /* 포트 목록 문자열 버퍼 크기 */

/* 명령행/메뉴에서 받은 모든 옵션 */
typedef struct {
    int  wgport;
    char subnet[32], serverip[32], clientip[32], dns[64], publicip[64];
    int  mtu;
    int  udp[MAX_PORTS], nudp, tcp[MAX_PORTS], ntcp;
    int  regenerate, purge, disable_routing, force, keep_config, no_auto_install;
    int  ports_given;               /* 명령행에서 -udp/-tcp 를 직접 줬는가 */
    char config[MAX_PATH], target[128], steamkey[128], wgver[32], extif[32];
    int  qport, timeout_ms;
} Opts;

extern Opts O;
extern char g_workdir[MAX_PATH];   /* Windows: C:\ProgramData\TunnelingGUI   Linux: /etc/tgui */
extern char g_wgdir[MAX_PATH];     /* Windows: C:\Program Files\WireGuard   Linux: /etc/wireguard */
extern char g_wg[MAX_PATH];        /* wg 실행 파일 (Linux 는 PATH 의 "wg") */
extern char g_wgexe[MAX_PATH];     /* Windows: wireguard.exe  Linux: "wg-quick" */

void opts_default(void);
void init_paths(void);             /* 플랫폼별 구현 (win/platform.c, posix/platform.c) */

#endif
