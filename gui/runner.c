/* runner.c
 * 명령 하나를 SDL 스레드에서 spawn_run() 으로 실행한다. 출력은 뮤텍스로 보호되는 버퍼에 쌓이고,
 * 화면(메인 스레드)이 매 프레임 runner_take() 로 가져간다.
 */
#include "runner.h"
#include "spawn.h"

static void on_output(const char *data, size_t len, void *ctx)
{
    Runner *r = (Runner *)ctx;
    SDL_LockMutex(r->lock);
    if (r->pending_len + len + 1 > r->pending_cap) {
        size_t cap = (r->pending_cap ? r->pending_cap * 2 : 8192);
        while (cap < r->pending_len + len + 1) cap *= 2;
        char *n = (char *)SDL_realloc(r->pending, cap);
        if (!n) { SDL_UnlockMutex(r->lock); return; }
        r->pending = n;
        r->pending_cap = cap;
    }
    SDL_memcpy(r->pending + r->pending_len, data, len);
    r->pending_len += len;
    r->pending[r->pending_len] = 0;
    SDL_UnlockMutex(r->lock);
}

static int SDLCALL thread_main(void *ctx)
{
    Runner *r = (Runner *)ctx;
    int code = spawn_run((const char *const *)r->args, on_output, r);
    if (code < 0) {
        const char *m = "\n[실행 실패] 프로그램을 시작하지 못했습니다: ";
        on_output(m, SDL_strlen(m), r);
        on_output(r->args[0], SDL_strlen(r->args[0]), r);
        on_output("\n", 1, r);
    }
    SDL_LockMutex(r->lock);
    r->exit_code = code;
    r->running = false;
    r->finished = true;
    SDL_UnlockMutex(r->lock);
    return 0;
}

bool runner_init(Runner *r)
{
    SDL_zerop(r);
    r->lock = SDL_CreateMutex();
    return r->lock != NULL;
}

static void free_args(Runner *r)
{
    for (int i = 0; i < RUNNER_MAX_ARGS && r->args[i]; i++) { SDL_free(r->args[i]); r->args[i] = NULL; }
}

void runner_quit(Runner *r)
{
    if (r->thread) SDL_WaitThread(r->thread, NULL);   /* 실행 중인 명령은 끝까지 기다린다 (중간에 끊으면 설정이 반쯤 남는다) */
    free_args(r);
    SDL_free(r->pending);
    if (r->lock) SDL_DestroyMutex(r->lock);
    SDL_zerop(r);
}

bool runner_busy(Runner *r)
{
    bool b;
    SDL_LockMutex(r->lock);
    b = r->running;
    SDL_UnlockMutex(r->lock);
    return b;
}

bool runner_start(Runner *r, const char *const *argv, const char *title)
{
    int i;
    if (runner_busy(r)) return false;
    if (r->thread) { SDL_WaitThread(r->thread, NULL); r->thread = NULL; }
    free_args(r);
    for (i = 0; argv[i] && i < RUNNER_MAX_ARGS; i++) r->args[i] = SDL_strdup(argv[i]);
    r->args[i] = NULL;
    SDL_strlcpy(r->title, title, sizeof r->title);
    r->running = true;
    r->finished = false;
    r->exit_code = 0;
    r->thread = SDL_CreateThread(thread_main, "tgui-runner", r);
    if (!r->thread) { r->running = false; return false; }
    return true;
}

char *runner_take(Runner *r, bool *done, int *code)
{
    char *out = NULL;
    SDL_LockMutex(r->lock);
    if (r->pending_len) {
        out = SDL_strdup(r->pending);
        r->pending_len = 0;
        r->pending[0] = 0;
    }
    *done = r->finished;
    *code = r->exit_code;
    r->finished = false;
    SDL_UnlockMutex(r->lock);
    return out;
}
