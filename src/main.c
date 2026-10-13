/* main.c
 * 진입점: 권한 확인(Windows: 관리자, Linux: root) 후 명령 모드 또는 메뉴 실행.
 */
#include "tgui.h"

int main(int argc, char **argv)
{
    int rc, has_cmd;

    console_init();
    opts_default();
    init_paths();

    /* 도움말은 권한 없이도 볼 수 있다 */
    if (argc > 1 && (flag(argv[1], "help") || flag(argv[1], "h") || !strcmp(argv[1], "/?"))) {
        usage();
        pause_if_own_console();
        return 0;
    }

    if (!is_admin()) {
        int elevated = 0;
        for (int i = 1; i < argc; i++) if (flag(argv[i], "elevated")) elevated = 1;
        if (elevated || !relaunch_elevated(argc, argv)) {
            say(C_RED, "%s\n", admin_hint());
            pause_if_own_console();
            return 1;
        }
        return 0; /* Windows: 상승된 새 프로세스가 이어서 실행 (Linux 는 sudo 로 exec 되어 여기 오지 않음) */
    }

    settings_load();   /* 이전에 저장한 포트 등을 기본값으로 (명령행 옵션이 우선) */
    has_cmd = argc > 1 && argv[1][0] != '-';
    if (has_cmd) {
        if (!parse_opts(argc, argv, 2) || !validate_opts()) { pause_if_own_console(); return 2; }
        rc = dispatch(argv[1]);
    } else {
        if (argc > 1 && !parse_opts(argc, argv, 1)) { pause_if_own_console(); return 2; }
        if (!validate_opts()) return 2;
        rc = menu();
    }
    pause_if_own_console();
    return rc;
}
