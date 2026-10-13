/* runner.h - CLI 명령을 백그라운드 스레드에서 실행하고 출력을 모은다 (화면은 멈추지 않는다) */
#ifndef TGUI_GUI_RUNNER_H
#define TGUI_GUI_RUNNER_H
#include <SDL3/SDL.h>

#define RUNNER_MAX_ARGS 48

typedef struct {
    SDL_Mutex  *lock;
    SDL_Thread *thread;
    char       *pending;        /* 아직 화면이 가져가지 않은 출력 */
    size_t      pending_len, pending_cap;
    bool        running;
    bool        finished;       /* 끝났고 결과를 아직 가져가지 않음 */
    int         exit_code;
    char       *args[RUNNER_MAX_ARGS + 1];
    char        title[256];     /* 화면 표시용 (예: "vps-setup") */
} Runner;

bool runner_init(Runner *r);
void runner_quit(Runner *r);
/* argv 를 복사해서 실행 시작. 이미 실행 중이면 false */
bool runner_start(Runner *r, const char *const *argv, const char *title);
bool runner_busy(Runner *r);
/* 새 출력을 가져간다 (malloc 한 문자열, 없으면 NULL). 끝났으면 *done=true, *code=종료 코드 */
char *runner_take(Runner *r, bool *done, int *code);

#endif
