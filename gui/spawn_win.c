/* spawn_win.c
 * Windows: CreateProcessW + 파이프. 콘솔 창이 뜨지 않게 CREATE_NO_WINDOW,
 * 한글 경로를 위해 UTF-8 → UTF-16 으로 바꿔서 실행한다.
 */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "spawn.h"

/* CommandLineToArgvW 규칙에 맞게 인자 하나를 따옴표로 감싸 붙인다 */
static void append_quoted(wchar_t *dst, size_t cap, const wchar_t *arg)
{
    size_t k = wcslen(dst);
    int need = (*arg == 0) || wcspbrk(arg, L" \t\"") != NULL;
    if (k && k + 1 < cap) dst[k++] = L' ';
    if (!need) {
        for (; *arg && k + 1 < cap; arg++) dst[k++] = *arg;
        dst[k] = 0;
        return;
    }
    if (k + 1 < cap) dst[k++] = L'"';
    for (const wchar_t *p = arg; *p && k + 4 < cap; p++) {
        size_t bs = 0;
        while (*p == L'\\') { bs++; p++; }
        if (!*p) { while (bs-- && k + 2 < cap) { dst[k++] = L'\\'; dst[k++] = L'\\'; } break; }
        if (*p == L'"') { bs = bs * 2 + 1; }
        while (bs-- && k + 2 < cap) dst[k++] = L'\\';
        dst[k++] = *p;
    }
    if (k + 1 < cap) dst[k++] = L'"';
    dst[k] = 0;
}

int spawn_run(const char *const *argv, SpawnOutputFn cb, void *ctx)
{
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE rd = NULL, wr = NULL, nul;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    wchar_t *cmd;
    size_t cap = 32768;
    DWORD got, code = (DWORD)-1;
    char buf[4096];

    cmd = (wchar_t *)calloc(cap, sizeof(wchar_t));
    if (!cmd) return -1;
    for (int i = 0; argv[i]; i++) {
        wchar_t w[4096];
        if (!MultiByteToWideChar(CP_UTF8, 0, argv[i], -1, w, 4096)) { free(cmd); return -1; }
        append_quoted(cmd, cap, w);
    }
    if (!CreatePipe(&rd, &wr, &sa, 0)) { free(cmd); return -1; }
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);

    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nul;          /* CLI 가 질문하면 바로 EOF → 기본값/취소 (GUI 는 -force 로 확인을 대신한다) */
    si.hStdOutput = wr;
    si.hStdError = wr;
    if (!CreateProcessW(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &si, &pi)) {
        free(cmd); CloseHandle(rd); CloseHandle(wr); if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
        return -1;
    }
    free(cmd);
    CloseHandle(wr);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);

    while (ReadFile(rd, buf, sizeof buf, &got, NULL) && got) cb(buf, got, ctx);
    CloseHandle(rd);
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}
#endif
