/* 테스트 전용: if_nametoindex() 를 가로채서 $TGUI_FAKE_IF_DIR/<이름> 파일이 있으면 "인터페이스 있음"으로 취급한다.
 * (CI/컨테이너에는 WireGuard 커널 모듈과 NET_ADMIN 권한이 없을 수 있어서 가짜 wg-quick 과 함께 쓴다) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

unsigned int if_nametoindex(const char *name)
{
    char path[512];
    const char *dir = getenv("TGUI_FAKE_IF_DIR");
    if (!dir) return 0;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    return access(path, F_OK) == 0 ? 1 : 0;
}
