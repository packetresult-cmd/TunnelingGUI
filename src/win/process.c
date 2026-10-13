/* win/process.c
 * 자식 프로세스 실행(출력 수집)과 PowerShell 호출 (Windows).
 */
#include "tgui.h"

BOOL run(const char *cmd, const char *input, char *out, size_t outsz, DWORD *code)
{
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE or_, ow, ir, iw;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char *cl;
    size_t used = 0;
    DWORD got;
    char tmp[1024];

    if (out && outsz) out[0] = 0;
    if (!CreatePipe(&or_, &ow, &sa, 0)) return FALSE;
    if (!CreatePipe(&ir, &iw, &sa, 0)) { CloseHandle(or_); CloseHandle(ow); return FALSE; }
    SetHandleInformation(or_, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(iw, HANDLE_FLAG_INHERIT, 0);

    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = si.hStdError = ow;
    si.hStdInput = ir;

    cl = _strdup(cmd);
    if (!cl) return FALSE;
    if (!CreateProcessA(NULL, cl, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        free(cl);
        CloseHandle(or_); CloseHandle(ow); CloseHandle(ir); CloseHandle(iw);
        return FALSE;
    }
    free(cl);
    CloseHandle(ow);
    CloseHandle(ir);
    if (input) WriteFile(iw, input, (DWORD)strlen(input), &got, NULL);
    CloseHandle(iw);

    while (ReadFile(or_, tmp, sizeof tmp, &got, NULL) && got) {
        if (out && used + got < outsz) { memcpy(out + used, tmp, got); used += got; out[used] = 0; }
    }
    CloseHandle(or_);
    WaitForSingleObject(pi.hProcess, INFINITE);
    if (code) GetExitCodeProcess(pi.hProcess, code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return TRUE;
}

BOOL run_ok(const char *cmd)
{
    DWORD code = 1;
    return run(cmd, NULL, NULL, 0, &code) && code == 0;
}

static BOOL b64(const unsigned char *d, size_t n, char *o)
{
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i, k = 0;
    for (i = 0; i + 2 < n; i += 3) {
        o[k++] = T[d[i] >> 2];
        o[k++] = T[((d[i] & 3) << 4) | (d[i + 1] >> 4)];
        o[k++] = T[((d[i + 1] & 15) << 2) | (d[i + 2] >> 6)];
        o[k++] = T[d[i + 2] & 63];
    }
    if (i < n) {
        o[k++] = T[d[i] >> 2];
        if (i + 1 < n) {
            o[k++] = T[((d[i] & 3) << 4) | (d[i + 1] >> 4)];
            o[k++] = T[(d[i + 1] & 15) << 2];
        } else {
            o[k++] = T[(d[i] & 3) << 4];
            o[k++] = '=';
        }
        o[k++] = '=';
    }
    o[k] = 0;
    return TRUE;
}

/* 실행할 powershell.exe 경로.
 * 64비트 Windows 에서 이 프로그램이 32비트(i686 빌드)이면 System32 가 SysWOW64 로 리디렉션되어 "32비트 PowerShell" 이 뜨는데,
 * 32비트 PowerShell 에서는 NetNat/NetTCPIP 등 64비트 WMI 공급자 클래스가 없어 "잘못된 클래스입니다(0x80041010)" 가 난다.
 * 그래서 32비트 프로세스는 Sysnative(= 진짜 System32) 경로로 64비트 PowerShell 을 띄운다. */
static void powershell_path(char *out, size_t sz)
{
    char root[MAX_PATH], sn[MAX_PATH];
    BOOL wow = FALSE;
    if (!GetEnvironmentVariableA("SystemRoot", root, sizeof root)) strcpy(root, "C:\\Windows");
    snprintf(out, sz, "%s\\System32\\WindowsPowerShell\\v1.0\\powershell.exe", root);
    IsWow64Process(GetCurrentProcess(), &wow);
    if (wow) {
        snprintf(sn, sizeof sn, "%s\\Sysnative\\WindowsPowerShell\\v1.0\\powershell.exe", root);
        if (GetFileAttributesA(sn) != INVALID_FILE_ATTRIBUTES) snprintf(out, sz, "%s", sn);
    }
}

/* PowerShell 스크립트를 -EncodedCommand 로 실행(따옴표 문제 회피). 메시지는 ASCII 로 유지할 것. 성공=TRUE */
BOOL ps(const char *script, char *out, size_t outsz)
{
    static char wrapped[16384];
    wchar_t *w;
    char *enc, *cmd, pshell[MAX_PATH];
    int wl;
    DWORD code = 1;
    BOOL ok;

    snprintf(wrapped, sizeof wrapped,
             "$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';"
             "try{[Console]::OutputEncoding=(New-Object System.Text.UTF8Encoding($false))}catch{};"
             /* PowerShell 7 / MSYS2 등 부모 셸이 물려준 PSModulePath 가 5.1 의 모듈 로딩을 방해하지 않게 시스템 기본값으로 */
             "$m=[Environment]::GetEnvironmentVariable('PSModulePath','Machine');if($m){$env:PSModulePath=$m};"
             /* 성공하면 명시적으로 exit 0: -ErrorAction SilentlyContinue 로 무시한 오류(예: "규칙 없음")가 있으면
              * $? 가 false 로 남아 powershell.exe 가 종료 코드 1 을 돌려주기 때문. 진짜 오류는 Stop 이라 catch 로 간다 */
             "try{%s\nexit 0}catch{[Console]::Out.WriteLine($_.Exception.Message);exit 1}", script);
    wl = MultiByteToWideChar(CP_UTF8, 0, wrapped, -1, NULL, 0);
    w = (wchar_t *)malloc((size_t)wl * sizeof(wchar_t));
    enc = (char *)malloc((size_t)wl * 4 + 16);
    cmd = (char *)malloc((size_t)wl * 4 + 600);
    if (!w || !enc || !cmd) { free(w); free(enc); free(cmd); return FALSE; }
    MultiByteToWideChar(CP_UTF8, 0, wrapped, -1, w, wl);
    b64((const unsigned char *)w, (size_t)(wl - 1) * sizeof(wchar_t), enc);

    powershell_path(pshell, sizeof pshell);
    snprintf(cmd, (size_t)wl * 4 + 600,
             "\"%s\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand %s", pshell, enc);
    ok = run(cmd, NULL, out, outsz, &code) && code == 0;
    free(w); free(enc); free(cmd);
    return ok;
}

/* 실패 시 PowerShell 출력까지 보여주는 래퍼 */
BOOL ps_step(const char *script, const char *what)
{
    char out[4096];
    if (ps(script, out, sizeof out)) return TRUE;
    trim(out);
    say(C_RED, "실패: %s - %s\n", what, *out ? out : "(PowerShell 이 오류 메시지 없이 실패했습니다)");
    return FALSE;
}
