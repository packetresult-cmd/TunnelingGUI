/* posix/pkg.c
 * 배포판 공식 패키지 관리자로 필요한 도구를 설치한다 (apt / dnf / yum / zypper / pacman / apk).
 * 외부 저장소는 추가하지 않는다.
 */
#include "tgui.h"
#include "pkg.h"

typedef struct {
    const char *cmd;
    const char *apt, *dnf, *zypper, *pacman, *apk;   /* 패키지 이름. 공백으로 구분된 후보는 앞에서부터 시도 */
} PkgMap;

static const PkgMap MAP[] = {
    { "wg",       "wireguard-tools", "wireguard-tools",         "wireguard-tools", "wireguard-tools", "wireguard-tools" },
    { "wg-quick", "wireguard-tools", "wireguard-tools",         "wireguard-tools", "wireguard-tools", "wireguard-tools" },
    { "iptables", "iptables",        "iptables-nft iptables",   "iptables",        "iptables",        "iptables" },
    { "ip",       "iproute2",        "iproute",                 "iproute2",        "iproute2",        "iproute2" },
    { "curl",     "curl",            "curl",                    "curl",            "curl",            "curl" },
    { "sysctl",   "procps",          "procps-ng",               "procps",          "procps-ng",       "procps" },
};

BOOL have_cmd(const char *cmd)
{
    char c[128];
    snprintf(c, sizeof c, "command -v %s >/dev/null 2>&1", cmd);
    return run_ok(c);
}

static BOOL install_one(const char *pm, const char *pkg)
{
    char cmd[512], out[2048];
    DWORD code = 1;
    if (!strcmp(pm, "apt-get")) snprintf(cmd, sizeof cmd, "DEBIAN_FRONTEND=noninteractive apt-get install -y %s", pkg);
    else if (!strcmp(pm, "dnf") || !strcmp(pm, "yum")) snprintf(cmd, sizeof cmd, "%s install -y %s", pm, pkg);
    else if (!strcmp(pm, "zypper")) snprintf(cmd, sizeof cmd, "zypper --non-interactive install %s", pkg);
    else if (!strcmp(pm, "pacman")) snprintf(cmd, sizeof cmd, "pacman -S --noconfirm --needed %s", pkg);
    else snprintf(cmd, sizeof cmd, "apk add %s", pkg);
    pf("  %s\n", cmd);
    if (run(cmd, NULL, out, sizeof out, &code) && code == 0) return TRUE;
    trim(out);
    warn("패키지 설치 실패(%s): %.300s", pkg, out);
    return FALSE;
}

BOOL ensure_cmd(const char *cmd)
{
    static const char *const PMS[] = { "apt-get", "dnf", "yum", "zypper", "pacman", "apk" };
    static BOOL updated = FALSE;
    const PkgMap *m = NULL;
    const char *pm = NULL, *names;
    char list[160], *tok, *save = NULL;
    size_t i;

    if (have_cmd(cmd)) return TRUE;
    for (i = 0; i < sizeof MAP / sizeof MAP[0]; i++) if (!strcmp(MAP[i].cmd, cmd)) m = &MAP[i];
    for (i = 0; i < sizeof PMS / sizeof PMS[0] && !pm; i++) if (have_cmd(PMS[i])) pm = PMS[i];
    if (!m || !pm) { say(C_RED, "'%s' 명령이 없고 자동 설치할 방법을 찾지 못했습니다. 직접 설치하세요.\n", cmd); return FALSE; }

    if (!strcmp(pm, "apt-get")) names = m->apt;
    else if (!strcmp(pm, "dnf") || !strcmp(pm, "yum")) names = m->dnf;
    else if (!strcmp(pm, "zypper")) names = m->zypper;
    else if (!strcmp(pm, "pacman")) names = m->pacman;
    else names = m->apk;

    if (!strcmp(pm, "apt-get") && !updated) { updated = TRUE; pf("  apt-get update\n"); run_ok("DEBIAN_FRONTEND=noninteractive apt-get update"); }

    snprintf(list, sizeof list, "%s", names);
    for (tok = strtok_r(list, " ", &save); tok; tok = strtok_r(NULL, " ", &save))
        if (install_one(pm, tok) && have_cmd(cmd)) return TRUE;
    say(C_RED, "'%s' 설치에 실패했습니다.\n", cmd);
    return FALSE;
}
