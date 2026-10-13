/* win/admin.c
 * 관리자 권한 확인과 runas 재실행.
 */
#include "tgui.h"
#include <shellapi.h>

BOOL is_admin(void)
{
    BOOL r = FALSE;
    PSID sid = NULL;
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &sid)) {
        if (!CheckTokenMembership(NULL, sid, &r)) r = FALSE;
        FreeSid(sid);
    }
    return r;
}

/* 매니페스트(requireAdministrator)가 무시된 경우를 위한 보조 수단: runas 로 자기 자신을 다시 실행 */
BOOL relaunch_elevated(int argc, char **argv)
{
    char exe[MAX_PATH], params[2048];
    const char *cl = GetCommandLineA();
    SHELLEXECUTEINFOA sei;
    (void)argc; (void)argv;
    if (*cl == '"') { cl++; while (*cl && *cl != '"') cl++; if (*cl) cl++; }
    else while (*cl && *cl != ' ') cl++;
    while (*cl == ' ') cl++;
    snprintf(params, sizeof params, "%s%s--elevated", cl, *cl ? " " : "");
    GetModuleFileNameA(NULL, exe, sizeof exe);
    memset(&sei, 0, sizeof sei);
    sei.cbSize = sizeof sei;
    sei.lpVerb = "runas";
    sei.lpFile = exe;
    sei.lpParameters = params;
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExA(&sei);
}

const char *admin_hint(void)
{
    return "관리자 권한이 필요합니다. '관리자 권한으로 실행' 하세요.";
}
