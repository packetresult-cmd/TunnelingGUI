/* tunnel.h - WireGuard 터널 (Windows: 터널 서비스, Linux: wg-quick@<name>) */
#ifndef TGUI_TUNNEL_H
#define TGUI_TUNNEL_H
#include "common.h"

const char *tunnel_status(const char *name);       /* "Running"/"Stopped"/..., 설치 안 됐으면 NULL */
#define tunnel_exists(name) (tunnel_status(name) != NULL)
BOOL tunnel_install(const char *name, const char *conf_path);  /* 설치 + 시작 (재부팅 후에도 유지) */
BOOL tunnel_remove(const char *name);              /* 중지 + 제거 */
BOOL tunnel_wait_ready(const char *name, int timeout_ms);      /* 네트워크 인터페이스가 올라올 때까지 */
long handshake_age(const char *name);              /* 마지막 핸드셰이크 이후 초, 없으면 -1 (공통 구현) */
void tunnel_diagnose(const char *name);            /* 실패 원인 출력 (서비스 상태, 로그 마지막 부분) */

#endif
