/* spawn_posix.c
 * Linux: fork + execvp + 파이프. 셸을 거치지 않으므로 인자 인용 문제가 없다.
 */
#ifndef _WIN32
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "spawn.h"

int spawn_run(const char *const *argv, SpawnOutputFn cb, void *ctx)
{
    int p[2], status = 0;
    pid_t pid;
    char buf[4096];
    ssize_t n;

    if (pipe(p) < 0) return -1;
    pid = fork();
    if (pid < 0) { close(p[0]); close(p[1]); return -1; }
    if (pid == 0) {
        int nul = open("/dev/null", O_RDONLY);
        if (nul >= 0) dup2(nul, STDIN_FILENO);   /* 질문이 나오면 바로 EOF */
        dup2(p[1], STDOUT_FILENO);
        dup2(p[1], STDERR_FILENO);
        close(p[0]);
        close(p[1]);
        execvp(argv[0], (char *const *)argv);
        {
            const char *m = "실행할 수 없습니다: ";
            if (write(STDOUT_FILENO, m, strlen(m)) < 0 || write(STDOUT_FILENO, argv[0], strlen(argv[0])) < 0) { }
        }
        _exit(127);
    }
    close(p[1]);
    while ((n = read(p[0], buf, sizeof buf)) > 0 || (n < 0 && errno == EINTR))
        if (n > 0) cb(buf, (size_t)n, ctx);
    close(p[0]);
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) { }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
}
#endif
