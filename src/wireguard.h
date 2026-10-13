/* wireguard.h - WireGuard 설치/키 */
#ifndef TGUI_WIREGUARD_H
#define TGUI_WIREGUARD_H
#include "common.h"

typedef struct { char priv[64], pub[64]; } KeyPair;

/* 플랫폼 구현 */
BOOL wg_installed(void);
BOOL install_wireguard(void);   /* Windows: 공식 MSI + 서명 검증 / Linux: 배포판 패키지 관리자 */
char *adapt_client_conf(const char *text);   /* 게임 PC 설정을 이 OS 에 맞게 보정 (free() 필요) */
/* 공통 */
BOOL ensure_wireguard(void);    /* 없으면 자동 설치 */
BOOL gen_keypair(KeyPair *k);   /* wg genkey / pubkey */

#endif
