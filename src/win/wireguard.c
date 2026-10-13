/* win/wireguard.c
 * WireGuard 설치 확인과 공식 MSI 자동 설치(Authenticode 서명 검증) (Windows).
 */
#include "tgui.h"
#include <wintrust.h>
#include <softpub.h>
#include <wincrypt.h>

BOOL wg_installed(void) { return file_exists(g_wg) && file_exists(g_wgexe); }

static const char *wg_arch(void)
{
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    switch (si.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_ARM64: return "arm64";
    case PROCESSOR_ARCHITECTURE_INTEL: return "x86";
    default: return "amd64";
    }
}

/* MSI 의 Authenticode 서명 검증 + 서명자가 WireGuard 인지 확인 */
static BOOL verify_signature(const char *path, char *signer, size_t sz)
{
    wchar_t wp[MAX_PATH];
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    BOOL ok = FALSE;
    signer[0] = 0;
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wp, MAX_PATH);

    for (int attempt = 0; attempt < 2 && !ok; attempt++) {
        WINTRUST_FILE_INFO fi;
        WINTRUST_DATA wd;
        LONG r;
        memset(&fi, 0, sizeof fi);
        fi.cbStruct = sizeof fi;
        fi.pcwszFilePath = wp;
        memset(&wd, 0, sizeof wd);
        wd.cbStruct = sizeof wd;
        wd.dwUIChoice = WTD_UI_NONE;
        wd.fdwRevocationChecks = attempt == 0 ? WTD_REVOKE_WHOLECHAIN : WTD_REVOKE_NONE;
        wd.dwUnionChoice = WTD_CHOICE_FILE;
        wd.pFile = &fi;
        wd.dwStateAction = WTD_STATEACTION_VERIFY;
        r = WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &action, &wd);
        if (r == 0) {
            CRYPT_PROVIDER_DATA *pd = WTHelperProvDataFromStateData(wd.hWVTStateData);
            CRYPT_PROVIDER_SGNR *sg = pd ? WTHelperGetProvSignerFromChain(pd, 0, FALSE, 0) : NULL;
            CRYPT_PROVIDER_CERT *pc = sg ? WTHelperGetProvCertFromChain(sg, 0) : NULL;
            if (pc && pc->pCert)
                CertGetNameStringA(pc->pCert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, signer, (DWORD)sz);
            ok = strstr(signer, "WireGuard") != NULL;
        }
        wd.dwStateAction = WTD_STATEACTION_CLOSE;
        WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &action, &wd);
        /* 폐지 서버에 닿지 못한 경우에만 폐지 검사 없이 재시도 */
        if (r == 0 || ((unsigned long)r != 0x80092013UL && (unsigned long)r != 0x80092012UL)) break;
        if (attempt == 0) warn("인증서 폐지 확인 서버에 연결할 수 없어 폐지 검사 없이 서명을 확인합니다.");
    }
    return ok;
}

static BOOL latest_wg_version(char *ver, size_t sz)
{
    char url[256], body[8192], pat[48], *p, *e;
    size_t n;
    snprintf(url, sizeof url, "%slatest.sig", WG_BASE_URL);
    if (!http_text(url, NULL, body, sizeof body, 8000)) return FALSE;
    snprintf(pat, sizeof pat, "wireguard-%s-", wg_arch());
    p = strstr(body, pat);
    if (!p) return FALSE;
    p += strlen(pat);
    e = strstr(p, ".msi");
    if (!e) return FALSE;
    n = (size_t)(e - p);
    if (!n || n >= sz) return FALSE;
    for (size_t i = 0; i < n; i++)
        if (!((p[i] >= '0' && p[i] <= '9') || p[i] == '.')) return FALSE;
    memcpy(ver, p, n);
    ver[n] = 0;
    return TRUE;
}

BOOL install_wireguard(void)
{
    char ver[32], url[256], tmpdir[MAX_PATH], msi[MAX_PATH], msilog[MAX_PATH], signer[256], cmd[3 * MAX_PATH + 96];
    Sink s;
    DWORD code = 1;
    BOOL ok;

    step("WireGuard 설치");
    if (*O.wgver) snprintf(ver, sizeof ver, "%s", O.wgver);
    else if (!latest_wg_version(ver, sizeof ver)) {
        snprintf(ver, sizeof ver, "%s", DEFAULT_WG_VER);
        warn("최신 버전을 조회하지 못해 기본 버전 %s 을 사용합니다.", ver);
    }
    snprintf(url, sizeof url, "%swireguard-%s-%s.msi", WG_BASE_URL, wg_arch(), ver);
    GetTempPathA(sizeof tmpdir, tmpdir);
    snprintf(msi, sizeof msi, "%stgui-wireguard-%s.msi", tmpdir, ver);

    pf("  %s\n", url);
    memset(&s, 0, sizeof s);
    s.fp = fopen(msi, "wb");
    if (!s.fp) { say(C_RED, "임시 파일을 만들 수 없습니다: %s\n", msi); return FALSE; }
    ok = http_get(url, NULL, &s, 30000, TRUE);
    fclose(s.fp);
    if (!ok) {
        DeleteFileA(msi);
        say(C_RED, "다운로드 실패. 인터넷 연결을 확인하거나 -wg-version 으로 버전을 지정하세요.\n");
        return FALSE;
    }

    pf("  서명 검증 중...\n");
    if (!verify_signature(msi, signer, sizeof signer)) {
        DeleteFileA(msi);
        say(C_RED, "서명 검증 실패. 설치를 중단합니다. (서명자: %s)\n", *signer ? signer : "없음");
        return FALSE;
    }
    pf("  서명자: %s\n", signer);

    pf("  설치 중...\n");
    /* DO_NOT_LAUNCH=1: 설치 직후 WireGuard UI/관리자 서비스를 SYSTEM 으로 자동 실행하지 않게 한다
     * (공식 문서 docs/enterprise.md 의 무인 설치 속성). 로그는 실패 원인 확인용 */
    snprintf(msilog, sizeof msilog, "%stgui-wireguard-install.log", tmpdir);
    snprintf(cmd, sizeof cmd, "msiexec.exe /i \"%s\" DO_NOT_LAUNCH=1 /qn /norestart /l*v \"%s\"", msi, msilog);
    ok = run(cmd, NULL, NULL, 0, &code) && (code == 0 || code == 3010);
    DeleteFileA(msi);
    if (!ok || !wg_installed()) {
        say(C_RED, "WireGuard 설치 실패 (msiexec 코드 %lu). 설치 로그: %s\n", (unsigned long)code, msilog);
        if (code == 1618) pf("  다른 설치 프로그램이 실행 중입니다. 끝난 뒤 다시 시도하세요.\n");
        if (code == 1603) pf("  설치 중 치명적 오류. 로그에서 'Return value 3' 근처를 확인하세요.\n");
        return FALSE;
    }
    if (code == 3010) warn("WireGuard 설치를 마치려면 재부팅이 필요할 수 있습니다.");
    say(C_GREEN, "WireGuard %s 설치 완료\n", ver);
    return TRUE;
}

/* Windows 에서는 게임 PC 설정을 그대로 쓴다 */
char *adapt_client_conf(const char *text)
{
    return _strdup(text);
}
