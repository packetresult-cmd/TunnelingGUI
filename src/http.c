/* http.c
 * 플랫폼과 무관한 HTTP 보조 함수 (텍스트 응답, 출구 IP 조회).
 * 실제 요청은 플랫폼 구현 http_get() 이 한다.
 */
#include "tgui.h"

BOOL http_text(const char *url, const char *hdrs, char *out, size_t sz, DWORD tmo)
{
    Sink s;
    memset(&s, 0, sizeof s);
    s.buf = out;
    s.cap = sz;
    if (sz) out[0] = 0;
    if (!http_get(url, hdrs, &s, tmo, FALSE)) return FALSE;
    trim(out);
    return TRUE;
}

BOOL egress_ip(char *out, size_t sz)
{
    static const char *urls[] = { "https://api.ipify.org", "https://ifconfig.me/ip", "https://icanhazip.com" };
    for (int i = 0; i < 3; i++)
        if (http_text(urls[i], NULL, out, sz, 5000) && *out) return TRUE;
    out[0] = 0;
    return FALSE;
}
