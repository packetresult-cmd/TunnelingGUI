/* posix/process.c
 * 자식 프로세스 실행(출력 수집). 명령은 /bin/sh -c 로 실행한다.
 * 호출하는 쪽은 사용자 입력을 반드시 검증한 뒤 명령 문자열에 넣을 것.
 */
#include "tgui.h"
#include <sys/wait.h>

BOOL run(const char *cmd, const char *input, char *out, size_t outsz, DWORD *code)
{
    int in_p[2], out_p[2];
    pid_t pid;
    size_t used = 0;
    ssize_t got;
    char tmp[1024];
    int status = 0;

    if (out && outsz) out[0] = 0;
    if (pipe(in_p) < 0) return FALSE;
    if (pipe(out_p) < 0) { close(in_p[0]); close(in_p[1]); return FALSE; }

    fflush(NULL);
    pid = fork();
    if (pid < 0) {
        close(in_p[0]); close(in_p[1]); close(out_p[0]); close(out_p[1]);
        return FALSE;
    }
    if (pid == 0) {
        dup2(in_p[0], STDIN_FILENO);
        dup2(out_p[1], STDOUT_FILENO);
        dup2(out_p[1], STDERR_FILENO);
        close(in_p[0]); close(in_p[1]); close(out_p[0]); close(out_p[1]);
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }
    close(in_p[0]);
    close(out_p[1]);
    if (input) {
        size_t n = strlen(input);
        if (write(in_p[1], input, n) < 0) { /* 자식이 먼저 끝난 경우 무시 */ }
    }
    close(in_p[1]);

    while ((got = read(out_p[0], tmp, sizeof tmp)) > 0) {
        if (out && used + (size_t)got < outsz) { memcpy(out + used, tmp, (size_t)got); used += (size_t)got; out[used] = 0; }
    }
    close(out_p[0]);
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) { }
    if (code) *code = WIFEXITED(status) ? (DWORD)WEXITSTATUS(status) : (DWORD)(128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0));
    return TRUE;
}

BOOL run_ok(const char *cmd)
{
    DWORD code = 1;
    return run(cmd, NULL, NULL, 0, &code) && code == 0;
}
