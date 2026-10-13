/* settings.c
 * 마지막으로 적용한 설정을 <작업 폴더>/settings.conf 에 key=value 로 저장한다.
 * 다음 실행 때 기본값으로 쓰여서, vps-setup 을 옵션 없이 다시 실행해도 열었던 포트가 유지된다.
 * 비밀 정보는 넣지 않는다(키는 keys.json). GUI 가 권한 없이 읽을 수 있도록 공개 권한으로 저장한다.
 */
#include "tgui.h"

void settings_path(char *out, size_t sz)
{
    join(out, sz, g_workdir, "settings.conf");
}

BOOL settings_save(const char *role, const char *public_ip)
{
    char p[MAX_PATH], udps[PORTS_STR_MAX], tcps[PORTS_STR_MAX], text[PORTS_STR_MAX * 2 + 1024];
    settings_path(p, sizeof p);
    ports_str(O.udp, O.nudp, udps, sizeof udps);
    ports_str(O.tcp, O.ntcp, tcps, sizeof tcps);
    snprintf(text, sizeof text,
             "# TunnelingGUI 설정 (tgui 이 저장. 직접 고치지 말고 'ports' 명령이나 GUI 를 쓰세요)" NL
             "role=%s" NL "udp=%s" NL "tcp=%s" NL "wg_port=%d" NL "subnet=%s" NL "server_ip=%s" NL
             "client_ip=%s" NL "dns=%s" NL "mtu=%d" NL "public_ip=%s" NL,
             role, udps, tcps, O.wgport, O.subnet, O.serverip, O.clientip, O.dns, O.mtu, public_ip ? public_ip : "");
    if (!write_file(p, text)) return FALSE;
    make_world_readable(p);
    return TRUE;
}

static BOOL get(const char *t, const char *key, char *out, size_t sz)
{
    char k[64];
    const char *p = t;
    size_t kl;
    snprintf(k, sizeof k, "%s=", key);
    kl = strlen(k);
    while (p && *p) {
        if (!strncmp(p, k, kl)) {
            size_t n = strcspn(p + kl, "\r\n");
            if (n >= sz) return FALSE;
            memcpy(out, p + kl, n);
            out[n] = 0;
            return TRUE;
        }
        p = strchr(p, '\n');
        if (p) p++;
    }
    return FALSE;
}

/* 길이가 맞을 때만 복사 (잘린 값을 쓰지 않는다) */
static void set_str(char *dst, size_t sz, const char *v)
{
    size_t n = strlen(v);
    if (n < sz) memcpy(dst, v, n + 1);
}

void settings_load(void)
{
    char p[MAX_PATH], v[PORTS_STR_MAX], *t;
    int arr[MAX_PORTS], n;
    settings_path(p, sizeof p);
    t = read_file(p);
    if (!t) return;
    if (get(t, "udp", v, sizeof v)) { if (!*v) O.nudp = 0; else if (parse_ports(v, arr, &n)) { memcpy(O.udp, arr, sizeof arr); O.nudp = n; } }
    if (get(t, "tcp", v, sizeof v)) { if (!*v) O.ntcp = 0; else if (parse_ports(v, arr, &n)) { memcpy(O.tcp, arr, sizeof arr); O.ntcp = n; } }
    if (get(t, "wg_port", v, sizeof v) && atoi(v) > 0 && atoi(v) < 65536) O.wgport = atoi(v);
    if (get(t, "subnet", v, sizeof v) && valid_subnet(v)) set_str(O.subnet, sizeof O.subnet, v);
    if (get(t, "server_ip", v, sizeof v) && valid_ip(v)) set_str(O.serverip, sizeof O.serverip, v);
    if (get(t, "client_ip", v, sizeof v) && valid_ip(v)) set_str(O.clientip, sizeof O.clientip, v);
    if (get(t, "dns", v, sizeof v) && valid_dns(v)) set_str(O.dns, sizeof O.dns, v);
    if (get(t, "mtu", v, sizeof v) && atoi(v) >= 576 && atoi(v) <= 1500) O.mtu = atoi(v);
    free(t);
}

BOOL settings_role(char *out, size_t sz)
{
    char p[MAX_PATH], *t;
    BOOL ok;
    settings_path(p, sizeof p);
    t = read_file(p);
    if (!t) return FALSE;
    ok = get(t, "role", out, sz) && *out;
    free(t);
    return ok;
}
