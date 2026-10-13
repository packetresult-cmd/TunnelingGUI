/* spawn.h - 외부 프로그램(tgui CLI)을 실행하고 출력을 실시간으로 받는다 */
#ifndef TGUI_GUI_SPAWN_H
#define TGUI_GUI_SPAWN_H
#include <stddef.h>

/* 출력 조각이 올 때마다 호출된다 (실행 스레드에서) */
typedef void (*SpawnOutputFn)(const char *data, size_t len, void *ctx);

/* argv[0] 을 실행하고 끝날 때까지 기다린다. stdout+stderr 를 cb 로 전달.
 * 반환: 프로세스 종료 코드, 실행 자체에 실패하면 -1. 문자열은 모두 UTF-8 */
int spawn_run(const char *const *argv, SpawnOutputFn cb, void *ctx);

#endif
