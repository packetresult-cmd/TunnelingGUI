/* gateway.h - VPS 게이트웨이 네트워크 설정 (NAT, 포워딩) 과 호스트 방화벽 */
#ifndef TGUI_GATEWAY_H
#define TGUI_GATEWAY_H
#include "common.h"

/* 설치 전 점검. 문제가 있으면 원인과 해결 방법을 출력하고 FALSE */
BOOL gateway_preflight(BOOL install_missing);   /* install_missing: 부족한 도구를 자동 설치(Linux) */
/* 서버 터널 설정의 [Interface] 에 덧붙일 줄 (Linux: PostUp/PostDown NAT 규칙, Windows: 없음) */
void gateway_conf_extra(char *out, size_t sz);
BOOL gateway_apply(void);              /* 터널이 올라온 뒤 호출: 포워딩 + NAT */
BOOL gateway_present(void);            /* 이 프로그램이 만든 NAT 구성이 남아 있는가 */
BOOL gateway_remove(void);             /* NAT 구성 제거 */
BOOL gateway_disable_routing(void);    /* IP 포워딩 끄기 */
void gateway_status(void);             /* NAT 상태 한 줄 출력 */
void gateway_list_ports(void);
int  gateway_enable_nat_feature(void);  /* Windows: WinNAT 을 쓰기 위한 Hyper-V 등 기능 켜기 (재부팅 필요) */         /* 실제로 적용된 포트 전달(DNAT) 목록 출력 */

const char *firewall_label(void);      /* 화면 표시용 이름 */
BOOL firewall_remove(void);
BOOL firewall_rule(const char *name, const char *proto, const char *ports);   /* ports: "7777,7778" */
int  fw_count(void);
void firewall_list(void);              /* 이 프로그램이 연 방화벽 규칙 목록 출력 */

#endif
