/* fonts.c
 * 글꼴 파일을 프로그램에 넣지 않고 OS 에 있는 한글 글꼴을 쓴다.
 *   Windows: 맑은 고딕 (모든 한국어/영문 Windows 10 이상에 기본 포함)
 *   Linux  : Noto Sans CJK / 나눔고딕 / 은 글꼴 (배포판 패키지 fonts-noto-cjk, fonts-nanum 등)
 */
#include <SDL3/SDL.h>
#include "fonts.h"

static bool exists(const char *p)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(p, &info) && info.type == SDL_PATHTYPE_FILE;
}

bool find_korean_font(char *out, size_t sz)
{
    const char *env = SDL_getenv("TGUI_GUI_FONT");
#ifdef _WIN32
    static const char *const names[] = { "malgun.ttf", "malgunsl.ttf", "gulim.ttc", "batang.ttc" };
    const char *windir = SDL_getenv("WINDIR");
#else
    static const char *const paths[] = {
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/google-noto-sans-cjk-fonts/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/nanum/NanumGothic.ttf",
        "/usr/share/fonts/nanum/NanumGothic.ttf",
        "/usr/share/fonts/naver-nanum/NanumGothic.ttf",
        "/usr/share/fonts/truetype/unfonts-core/UnDotum.ttf",
        "/usr/share/fonts/truetype/baekmuk/gulim.ttf",
    };
#endif
    if (env && *env && exists(env)) { SDL_strlcpy(out, env, sz); return true; }
#ifdef _WIN32
    for (size_t i = 0; i < SDL_arraysize(names); i++) {
        SDL_snprintf(out, sz, "%s\\Fonts\\%s", windir ? windir : "C:\\Windows", names[i]);
        if (exists(out)) return true;
    }
#else
    for (size_t i = 0; i < SDL_arraysize(paths); i++)
        if (exists(paths[i])) { SDL_strlcpy(out, paths[i], sz); return true; }
#endif
    out[0] = 0;
    return false;
}
