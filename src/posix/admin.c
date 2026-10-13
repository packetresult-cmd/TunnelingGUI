/* posix/admin.c
 * root 권한 확인과 sudo 재실행.
 */
#include "tgui.h"
#include "pkg.h"

BOOL is_admin(void)
{
    return geteuid() == 0;
}

/* sudo 로 자기 자신을 다시 실행한다. 성공하면 exec 되어 돌아오지 않는다 */
BOOL relaunch_elevated(int argc, char **argv)
{
    char exe[MAX_PATH];
    char **nargv;
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    int i, k = 0;

    if (n <= 0 || !have_cmd("sudo")) return FALSE;
    exe[n] = 0;
    nargv = (char **)calloc((size_t)argc + 4, sizeof(char *));
    if (!nargv) return FALSE;
    nargv[k++] = (char *)"sudo";
    nargv[k++] = exe;
    for (i = 1; i < argc; i++) nargv[k++] = argv[i];
    nargv[k++] = (char *)"--elevated";
    nargv[k] = NULL;
    execvp("sudo", nargv);
    free(nargv);
    return FALSE;
}

const char *admin_hint(void)
{
    return "root 권한이 필요합니다. 'sudo' 로 실행하세요.";
}
