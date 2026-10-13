/* win/httpget.c
 * WinHTTP 기반 GET 요청 (Windows).
 */
#include "tgui.h"
#include <winhttp.h>


BOOL http_get(const char *url, const char *hdrs, Sink *s, DWORD tmo, BOOL progress)
{
    wchar_t wurl[1024], host[256], path[1024], wh2[512];
    URL_COMPONENTS uc;
    HINTERNET ses = NULL, con = NULL, req = NULL;
    DWORD status = 0, sl = sizeof status, got, total = 0, tl = sizeof total, done = 0;
    char tmp[16384];
    BOOL ok = FALSE;
    int last = -1;

    MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 1024);
    memset(&uc, 0, sizeof uc);
    uc.dwStructSize = sizeof uc;
    uc.lpszHostName = host;  uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;   uc.dwUrlPathLength = 1024;
    if (!WinHttpCrackUrl(wurl, 0, 0, &uc)) return FALSE;

    ses = WinHttpOpen(L"TunnelingGUI/" L"1.0", strncmp(url, "http://169.254.", 15) ? WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY : WINHTTP_ACCESS_TYPE_NO_PROXY,
                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) return FALSE;
    WinHttpSetTimeouts(ses, (int)tmo, (int)tmo, (int)tmo, (int)tmo);
    con = WinHttpConnect(ses, host, uc.nPort, 0);
    if (!con) goto end;
    req = WinHttpOpenRequest(con, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                             uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (!req) goto end;
    if (hdrs) {
        MultiByteToWideChar(CP_UTF8, 0, hdrs, -1, wh2, 512);
        if (!WinHttpAddRequestHeaders(req, wh2, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD)) goto end;
    }
    if (!WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) goto end;
    if (!WinHttpReceiveResponse(req, NULL)) goto end;
    if (!WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &sl, NULL) || status != 200) goto end;
    WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, NULL, &total, &tl, NULL);

    for (;;) {
        if (!WinHttpReadData(req, tmp, sizeof tmp, &got)) goto end;
        if (!got) break;
        done += got;
        if (s->fp) {
            if (fwrite(tmp, 1, got, s->fp) != got) goto end;
        } else if (s->len + got < s->cap) {
            memcpy(s->buf + s->len, tmp, got);
            s->len += got;
            s->buf[s->len] = 0;
        }
        if (progress && total) {
            int pc = (int)((unsigned long long)done * 100 / total);
            if (pc != last && pc % 5 == 0) { pf("\r  다운로드 %3d%%", pc); last = pc; }
        }
    }
    if (progress) pf("\n");
    ok = TRUE;
end:
    if (req) WinHttpCloseHandle(req);
    if (con) WinHttpCloseHandle(con);
    if (ses) WinHttpCloseHandle(ses);
    return ok;
}
