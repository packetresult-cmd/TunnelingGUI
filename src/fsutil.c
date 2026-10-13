/* fsutil.c
 * 플랫폼과 무관한 파일 유틸 (읽기, 경로 결합, 설정 폴더 준비).
 */
#include "tgui.h"

char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    long sz;
    size_t got;
    char *b;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) || (sz = ftell(f)) < 0 || sz > (1L << 20) || fseek(f, 0, SEEK_SET)) { fclose(f); return NULL; }
    b = (char *)malloc((size_t)sz + 1);
    if (!b) { fclose(f); return NULL; }
    got = fread(b, 1, (size_t)sz, f);
    fclose(f);
    b[got] = 0;
    return b;
}

void join(char *out, size_t sz, const char *a, const char *b)
{
    snprintf(out, sz, "%s%s%s", a, PATH_SEP, b);
}

BOOL ensure_workdir(void)
{
    if (!make_dir(g_workdir) || !file_exists(g_workdir)) {
        say(C_RED, "폴더를 만들 수 없습니다: %s\n", g_workdir);
        return FALSE;
    }
    lock_dir(g_workdir);
    return TRUE;
}
