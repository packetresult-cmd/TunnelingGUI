/* process.h - 외부 프로세스 실행 */
#ifndef TGUI_PROCESS_H
#define TGUI_PROCESS_H
#include "common.h"

/* cmd 실행, stdout+stderr 를 out 에 수집. input 이 있으면 stdin 으로 전달.
 * 프로세스를 실행했는지 여부 반환(종료 코드는 *code). POSIX 에서는 /bin/sh -c 로 실행 */
BOOL run(const char *cmd, const char *input, char *out, size_t outsz, DWORD *code);
/* 명령이 성공(종료코드 0)하면 TRUE */
BOOL run_ok(const char *cmd);

#ifdef _WIN32
/* PowerShell 스크립트 실행(-EncodedCommand). 성공(종료코드 0)=TRUE. 메시지는 ASCII 로 유지할 것 */
BOOL ps(const char *script, char *out, size_t outsz);
/* ps() + 실패 시 이유를 화면에 출력 */
BOOL ps_step(const char *script, const char *what);
#endif

#endif
