/* cmd_verify.c
 * 명령: verify (A2S_INFO 쿼리, Steam 마스터 목록).
 */
#include "tgui.h"

/* 소켓 API 의 Windows / POSIX 차이를 여기서만 흡수한다 */
#ifdef _WIN32
#  define SOCK_INIT(wd)        (WSAStartup(MAKEWORD(2, 2), &(wd)) == 0)
#  define SOCK_CLEANUP()       WSACleanup()
#  define SOCK_SET_TIMEOUT(s, ms) do { DWORD t_ = (DWORD)(ms); setsockopt((s), SOL_SOCKET, SO_RCVTIMEO, (const char *)&t_, sizeof t_); } while (0)
   typedef WSADATA SockInit;
#else
#  include <sys/time.h>
#  define SOCK_INIT(wd)        ((void)(wd), 1)
#  define SOCK_CLEANUP()       ((void)0)
#  define SOCK_SET_TIMEOUT(s, ms) do { struct timeval tv_; tv_.tv_sec = (ms) / 1000; tv_.tv_usec = ((ms) % 1000) * 1000; setsockopt((s), SOL_SOCKET, SO_RCVTIMEO, &tv_, sizeof tv_); } while (0)
#  define closesocket          close
#  define INVALID_SOCKET       (-1)
   typedef int SOCKET;
   typedef int SockInit;
#endif

static BOOL rd_cstr(const unsigned char *b, int len, int *pos, char *out, size_t sz)
{
    int s = *pos;
    while (*pos < len && b[*pos]) (*pos)++;
    if (*pos >= len) return FALSE;
    snprintf(out, sz, "%.*s", (int)((size_t)(*pos - s) < sz - 1 ? (size_t)(*pos - s) : sz - 1), (const char *)b + s);
    (*pos)++;
    return TRUE;
}

static BOOL a2s_info(const char *host, int port, int tmo, char *why, size_t wsz)
{
    struct addrinfo hints, *ai = NULL;
    char ps_[16], name[256], map[128], folder[128], game[128];
    unsigned char req[64], resp[2048];
    int reqlen, n = 0, pos;
    SOCKET so;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    snprintf(ps_, sizeof ps_, "%d", port);
    if (getaddrinfo(host, ps_, &hints, &ai) != 0 || !ai) { snprintf(why, wsz, "주소를 확인할 수 없습니다"); return FALSE; }
    so = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (so == INVALID_SOCKET) { freeaddrinfo(ai); snprintf(why, wsz, "소켓 생성 실패"); return FALSE; }
    SOCK_SET_TIMEOUT(so, tmo);

    memcpy(req, "\xFF\xFF\xFF\xFF\x54Source Engine Query", 24);
    req[24] = 0;
    reqlen = 25;
    for (int tryn = 0; tryn < 2; tryn++) {
        if (sendto(so, (const char *)req, reqlen, 0, ai->ai_addr, (int)ai->ai_addrlen) < 0) { snprintf(why, wsz, "전송 실패"); goto fail; }
        n = recv(so, (char *)resp, sizeof resp, 0);
        if (n < 0) { snprintf(why, wsz, "응답 시간 초과"); goto fail; }
        if (n >= 9 && resp[4] == 0x41) { memcpy(req + 25, resp + 5, 4); reqlen = 29; continue; }
        break;
    }
    if (n < 6 || resp[4] != 0x49) { snprintf(why, wsz, "예상하지 못한 응답 (type=0x%02X)", n > 4 ? resp[4] : 0); goto fail; }
    pos = 6;
    if (!rd_cstr(resp, n, &pos, name, sizeof name) || !rd_cstr(resp, n, &pos, map, sizeof map) ||
        !rd_cstr(resp, n, &pos, folder, sizeof folder) || !rd_cstr(resp, n, &pos, game, sizeof game) || pos + 4 > n) {
        snprintf(why, wsz, "응답 형식 오류");
        goto fail;
    }
    pos += 2;
    pf("  Name    : %s\n  Map     : %s\n  Folder  : %s\n  Game    : %s\n  Players : %d/%d\n", name, map, folder, game, resp[pos], resp[pos + 1]);
    closesocket(so);
    freeaddrinfo(ai);
    return TRUE;
fail:
    closesocket(so);
    freeaddrinfo(ai);
    return FALSE;
}

static BOOL json_field(const char *seg, const char *segend, const char *key, char *out, size_t sz)
{
    char pat[48];
    const char *p, *q;
    snprintf(pat, sizeof pat, "\"%s\"", key);
    p = strstr(seg, pat);
    if (!p || p >= segend) return FALSE;
    p = strchr(p + strlen(pat), ':');
    if (!p) return FALSE;
    p++;
    while (*p == ' ') p++;
    if (*p == '"') { p++; q = strchr(p, '"'); } else { q = p; while (*q && *q != ',' && *q != '}' && *q != '\n') q++; }
    if (!q || (size_t)(q - p) >= sz) return FALSE;
    memcpy(out, p, (size_t)(q - p));
    out[q - p] = 0;
    return TRUE;
}

int cmd_verify(void)
{
    SockInit wd;
    char why[128], *body, url[512], enc[256], addr_[64], name[256], map[96], pl[16], mx[16];
    int found = 0, rc = 0;
    Sink s;
    const char *p, *nx;
    size_t i;

    if (!*O.target) { say(C_RED, "-target 으로 확인할 주소(10.8.0.2 또는 VPS 공인 IP)를 지정하세요.\n"); return 1; }
    for (i = 0; O.target[i]; i++)
        if (!(O.target[i] == '.' || O.target[i] == '-' || (O.target[i] >= '0' && O.target[i] <= '9') ||
              (O.target[i] >= 'a' && O.target[i] <= 'z') || (O.target[i] >= 'A' && O.target[i] <= 'Z'))) {
            say(C_RED, "대상 주소 형식이 올바르지 않습니다.\n");
            return 1;
        }
    memset(&wd, 0, sizeof wd);
    if (!SOCK_INIT(wd)) { say(C_RED, "소켓 초기화 실패\n"); return 1; }

    step("A2S_INFO %s:%d", O.target, O.qport);
    if (a2s_info(O.target, O.qport, O.timeout_ms, why, sizeof why)) {
        say(C_GREEN, "  OK: 쿼리 응답 수신\n");
    } else {
        warn("  응답 없음: %s", why);
        pf("  · 10.8.0.2 대상 실패 → 터널 핸드셰이크, 게임 PC 방화벽, ARK 서버 실행/쿼리 포트 확인\n");
        pf("  · VPS 공인IP 대상 실패 → Azure NSG, VPS 방화벽, NAT 정적 매핑 확인\n");
        rc = 1;
    }
    SOCK_CLEANUP();

    if (*O.steamkey) {
        for (i = 0; O.steamkey[i]; i++)
            if (!((O.steamkey[i] >= '0' && O.steamkey[i] <= '9') || (O.steamkey[i] >= 'A' && O.steamkey[i] <= 'Z') || (O.steamkey[i] >= 'a' && O.steamkey[i] <= 'z'))) {
                say(C_RED, "Steam API 키 형식이 올바르지 않습니다.\n");
                return 1;
            }
        step("Steam 마스터 목록 조회 (gameaddr=%s)", O.target);
        snprintf(enc, sizeof enc, "%%5Cgameaddr%%5C%s", O.target);
        snprintf(url, sizeof url, "https://api.steampowered.com/IGameServersService/GetServerList/v1/?key=%s&filter=%s&limit=20", O.steamkey, enc);
        body = (char *)malloc(262144);
        if (!body) return 1;
        memset(&s, 0, sizeof s); s.buf = body; s.cap = 262144; body[0] = 0;
        if (!http_get(url, NULL, &s, 10000, FALSE)) {
            warn("  Steam API 호출 실패 (키/네트워크 확인)");
            free(body);
            return 1;
        }
        for (p = strstr(body, "\"addr\""); p; p = nx) {
            nx = strstr(p + 6, "\"addr\"");
            if (!nx) nx = p + strlen(p);
            if (json_field(p, nx, "addr", addr_, sizeof addr_)) {
                name[0] = map[0] = pl[0] = mx[0] = 0;
                json_field(p, nx, "name", name, sizeof name);
                json_field(p, nx, "map", map, sizeof map);
                json_field(p, nx, "players", pl, sizeof pl);
                json_field(p, nx, "max_players", mx, sizeof mx);
                pf("  %s  %s  [%s]  %s/%s\n", addr_, name, map, pl, mx);
                found++;
            }
            if (!*nx) break;
        }
        free(body);
        if (found) say(C_GREEN, "  OK: 목록에 등록되어 있습니다.\n");
        else {
            warn("  목록에 없습니다. 서버 시작 직후라면 5~10분 뒤 다시 확인하세요.");
            pf("  · 게임 PC에서 '상태'로 출구 IP가 VPS IP인지 확인\n");
            rc = 1;
        }
    }
    return rc;
}
