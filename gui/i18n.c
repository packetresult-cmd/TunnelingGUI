/* i18n.c - JSON-backed UI strings. See i18n.h.
 *
 * No external library: a small flat-object JSON reader and an open-addressing
 * hash table. English is compiled in and is always the fallback.
 */
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "i18n.h"

/* ------------------------------------------------------------------ built-in English
 * This table is the single source of truth. lang/en.json is generated from it by
 * tools/check_lang.py, which also fails CI if the two drift apart.
 */
typedef struct { const char *k, *v; } Pair;

static const Pair EN[] = {
    { "_name",                  "English" },

    /* header */
    { "env.win",                "Windows \xc2\xb7 Administrator" },
    { "env.linux",              "Linux \xc2\xb7 %s" },
    { "env.linux.root",         "root" },
    { "env.linux.pkexec",       "pkexec per action" },
    { "env.linux.noroot",       "not root" },
    { "lang.label",             "Language" },

    /* CLI execution */
    { "cli.missing.short",      "tgui executable not found" },
    { "cli.missing",            "tgui executable not found. Put it in the same folder as the GUI." },
    { "cli.missing.err",        "Error: tgui executable not found. Put it in the same folder as the GUI." },
    { "cli.needroot",           "Error: root privileges are required. Run 'sudo tgui-gui', or install polkit (pkexec)." },
    { "cli.threadfail",         "Error: could not start the worker thread." },
    { "cli.running",            "Running: %s ..." },
    { "cli.done",               "%s: done" },
    { "cli.failed",             "%s: failed (exit code %d) - check the log above" },
    { "exec.task",              "Test run" },

    /* message box */
    { "dlg.cancel",             "Cancel" },
    { "dlg.continue",           "Continue" },
    { "quit.busy",              "The window will close when the running task finishes. (Interrupting it can leave the configuration half-applied.)" },
    { "font.missing",           "No usable font was found.\nLinux: install the fonts-noto-cjk or fonts-nanum package, or point TGUI_GUI_FONT at a font file." },
    { "i18n.rejected",          "%d translated strings were ignored: their format placeholders do not match English." },

    /* tabs */
    { "tab.vps",                "VPS (gateway)" },
    { "tab.gamepc",             "Game PC" },
    { "tab.verify",             "Verify" },

    /* open ports panel */
    { "ports.title",            "Open ports (VPS public IP -> game PC)" },
    { "ports.p7777",            "7777  Game (UDP)" },
    { "ports.p7778",            "7778  Raw UDP (UDP)" },
    { "ports.p27015",           "27015  Server list query (UDP)" },
    { "ports.p27020",           "27020  RCON (TCP, optional)" },
    { "ports.extraudp",         "Extra UDP ports (lists and ranges, up to 256)" },
    { "ports.extraudp.hint",    "e.g. 7779-7782,27016-27017  (several servers)" },
    { "ports.extratcp",         "Extra TCP ports" },
    { "ports.extratcp.hint",    "e.g. 27021" },
    { "ports.udpcount",         "Open UDP, %d: %s" },
    { "ports.tcpcount",         "Open TCP, %d: %s" },
    { "ports.none",             "(none)" },
    { "ports.nonebare",         "none" },
    { "ports.applied",          "Currently applied (%s): UDP %s / TCP %s" },
    { "ports.notapplied",       "Nothing applied yet (settings are saved when you install)" },
    { "ports.apply",            "Apply port changes only" },
    { "ports.apply.task",       "Port change" },
    { "ports.note",             "After changing ports, open the same ports on the VPS, on the game PC, and in your cloud firewall (Azure NSG and the like)." },
    { "ports.err.udp",          "Extra UDP: %s" },
    { "ports.err.tcp",          "Extra TCP: %s" },
    { "ports.err.fix",          "Fix the port input first." },

    /* VPS tab */
    { "vps.title",              "VPS (gateway)" },
    { "vps.publicip",           "VPS public IP" },
    { "vps.publicip.hint",      "leave empty to auto-detect" },
    { "vps.wgport",             "WireGuard port" },
    { "vps.check",              "1. Pre-flight check" },
    { "vps.setup",              "2. Install / re-apply" },
    { "vps.check.task",         "Pre-flight check" },
    { "vps.setup.task",         "VPS install" },
    { "vps.badfield",           "The VPS public IP or the WireGuard port is not in a valid format." },
    { "vps.status",             "Show status" },
    { "vps.ports",              "Show ports" },
    { "vps.status.task",        "VPS status" },
    { "vps.ports.task",         "Show ports" },
    { "vps.purge",              "Delete keys and settings too" },
    { "vps.disablerouting",     "Turn IP forwarding off" },
    { "vps.uninstall",          "Remove VPS configuration" },
    { "vps.uninstall.task",     "VPS removal" },
    { "vps.uninstall.confirm",  "Have you already stopped or removed the tunnel on the game PC?\nRemoving the VPS first will cut the game PC's internet connection." },
    { "vps.note",               "A Linux VPS is recommended (Windows needs WinNAT). Allow the WireGuard port and everything under 'Open ports' in your cloud firewall." },

    /* Game PC tab */
    { "gamepc.title",           "Game PC (game server)" },
    { "gamepc.conf",            "client-gamepc.conf (created by the VPS install)" },
    { "gamepc.conf.hint",       "file path" },
    { "gamepc.browse",          "Browse" },
    { "gamepc.filter.conf",     "WireGuard config (*.conf)" },
    { "gamepc.filter.all",      "All files" },
    { "gamepc.install",         "1. Install config" },
    { "gamepc.up",              "2. Tunnel up" },
    { "gamepc.install.task",    "Game PC config install" },
    { "gamepc.up.task",         "Tunnel up" },
    { "gamepc.noconf",          "Choose the client-gamepc.conf path." },
    { "gamepc.badpath",         "The path cannot contain a double-quote character." },
    { "gamepc.down",            "Tunnel down" },
    { "gamepc.status",          "Show status" },
    { "gamepc.down.task",       "Tunnel down" },
    { "gamepc.status.task",     "Game PC status" },
    { "gamepc.keepconfig",      "Keep the settings folder (private key)" },
    { "gamepc.uninstall",       "Remove game PC configuration" },
    { "gamepc.uninstall.task",  "Game PC removal" },
    { "gamepc.uninstall.confirm", "Have you stopped the game server first?\nRemoving this drops the server from the Steam list." },
    { "gamepc.note",            "Add MultiHome=10.8.0.2 to the server's launch arguments. Use 'Tunnel down' to return to your home connection when you are not hosting." },

    /* Verify tab */
    { "verify.title",           "Verify (server query / Steam list)" },
    { "verify.target",          "Address to check" },
    { "verify.target.hint",     "10.8.0.2 from the VPS, the VPS public IP from outside" },
    { "verify.qport",           "Query port" },
    { "verify.steamkey",        "Steam Web API key (optional, checks Steam list registration)" },
    { "verify.run",             "Run verification" },
    { "verify.task",            "Verification" },
    { "verify.badfield",        "Check the format of the address, port and API key." },
    { "verify.wginstall",       "Install / check WireGuard" },
    { "verify.wginstall.task",  "WireGuard install" },
    { "verify.wintools",        "Windows VPS tools" },
    { "verify.winnat.note",     "If the VPS install fails with 'New-NetNat ... Invalid class', WinNAT management is turned off on this Windows. Use the button below to enable Hyper-V (HypervisorPlatform on Home editions), then reboot." },
    { "verify.winnat.btn",      "Enable WinNAT (reboot required)" },
    { "verify.winnat.confirm",  "This enables Hyper-V (HypervisorPlatform on Home editions). A reboot is required, and it can change how other virtualization software such as VirtualBox or VMware behaves." },
    { "verify.winnat.task",     "Enable WinNAT" },

    /* log panel */
    { "log.title",              "Log" },
    { "log.clear",              "Clear" },
    { "log.copy",               "Copy" },
    { "log.empty",              "Press a button and the output appears here." },

    /* demo mode (--demo) */
    { "demo.status",            "Done (exit code 0)" },
};

#define EN_COUNT ((int)(sizeof EN / sizeof EN[0]))

int         i18n_builtin_count(void)      { return EN_COUNT; }
const char *i18n_builtin_key(int i)       { return (i >= 0 && i < EN_COUNT) ? EN[i].k : NULL; }
const char *i18n_builtin_value(int i)     { return (i >= 0 && i < EN_COUNT) ? EN[i].v : NULL; }

/* ------------------------------------------------------------------ format signature */

char *i18n_fmt_sig(const char *s, char *out, size_t sz)
{
    size_t n = 0;
    if (!sz) return out;
    out[0] = 0;
    for (; s && *s; s++) {
        if (*s != '%') continue;
        if (s[1] == '%') { s++; continue; }
        /* copy the whole conversion specification up to and including its type char */
        if (n + 1 < sz) out[n++] = '%';
        s++;
        while (*s && !strchr("diouxXeEfgGaAcspn", *s)) {
            if (n + 1 < sz) out[n++] = *s;
            s++;
        }
        if (!*s) break;
        if (n + 1 < sz) out[n++] = *s;
    }
    out[n < sz ? n : sz - 1] = 0;
    return out;
}

/* ------------------------------------------------------------------ JSON (flat object of string->string) */

static int hex4(const char *p)
{
    int v = 0;
    for (int i = 0; i < 4; i++) {
        int c = (unsigned char)p[i], d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return -1;
        v = v * 16 + d;
    }
    return v;
}

static void utf8_put(char **o, unsigned cp)
{
    char *p = *o;
    if (cp < 0x80) { *p++ = (char)cp; }
    else if (cp < 0x800) { *p++ = (char)(0xC0 | (cp >> 6)); *p++ = (char)(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { *p++ = (char)(0xE0 | (cp >> 12)); *p++ = (char)(0x80 | ((cp >> 6) & 0x3F)); *p++ = (char)(0x80 | (cp & 0x3F)); }
    else { *p++ = (char)(0xF0 | (cp >> 18)); *p++ = (char)(0x80 | ((cp >> 12) & 0x3F)); *p++ = (char)(0x80 | ((cp >> 6) & 0x3F)); *p++ = (char)(0x80 | (cp & 0x3F)); }
    *o = p;
}

/* Reads one JSON string starting at *p (which must point at the opening quote)
   into `out`. Returns false on malformed input. */
static bool read_string(const char **p, char *out, size_t sz)
{
    const char *s = *p;
    char *o = out;
    char *end = out + sz - 5;            /* room for the longest UTF-8 sequence + NUL */
    if (*s != '"') return false;
    s++;
    while (*s && *s != '"') {
        if (o >= end) return false;
        if (*s != '\\') { *o++ = *s++; continue; }
        s++;
        switch (*s) {
        case 'n':  *o++ = '\n'; s++; break;
        case 't':  *o++ = '\t'; s++; break;
        case 'r':  *o++ = '\r'; s++; break;
        case 'b':  *o++ = '\b'; s++; break;
        case 'f':  *o++ = '\f'; s++; break;
        case '"':  *o++ = '"';  s++; break;
        case '\\': *o++ = '\\'; s++; break;
        case '/':  *o++ = '/';  s++; break;
        case 'u': {
            int hi, lo;
            unsigned cp;
            if ((hi = hex4(s + 1)) < 0) return false;
            s += 5;
            cp = (unsigned)hi;
            if (hi >= 0xD800 && hi <= 0xDBFF && s[0] == '\\' && s[1] == 'u' && (lo = hex4(s + 2)) >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000u + (((unsigned)hi - 0xD800u) << 10) + ((unsigned)lo - 0xDC00u);
                s += 6;
            }
            utf8_put(&o, cp);
            break;
        }
        default: return false;
        }
    }
    if (*s != '"') return false;
    *o = 0;
    *p = s + 1;
    return true;
}

static void skip_ws(const char **p) { while (**p == ' ' || **p == '\t' || **p == '\r' || **p == '\n') (*p)++; }

bool i18n_parse(const char *json, void (*cb)(void *ud, const char *k, const char *v), void *ud)
{
    static char key[256], val[2048];
    const char *p = json;
    if (!p) return false;
    skip_ws(&p);
    if (*p != '{') return false;
    p++;
    skip_ws(&p);
    if (*p == '}') return true;
    for (;;) {
        skip_ws(&p);
        if (!read_string(&p, key, sizeof key)) return false;
        skip_ws(&p);
        if (*p != ':') return false;
        p++;
        skip_ws(&p);
        if (!read_string(&p, val, sizeof val)) return false;
        if (cb) cb(ud, key, val);
        skip_ws(&p);
        if (*p == ',') { p++; continue; }
        if (*p == '}') { p++; skip_ws(&p); return *p == 0; }
        return false;
    }
}

/* ------------------------------------------------------------------ table */

#define SLOTS 512                        /* power of two, > 2x the number of keys */

typedef struct { const char *k; char *v; } Slot;

static Slot  g_tab[SLOTS];
static char  g_lang[16] = "en";
static int   g_rejected;

static unsigned hash(const char *s)
{
    unsigned h = 2166136261u;
    for (; *s; s++) { h ^= (unsigned char)*s; h *= 16777619u; }
    return h;
}

/* Finds the slot for `k`: either the one holding it, or the first free one. */
static Slot *slot_of(const char *k)
{
    unsigned i = hash(k) & (SLOTS - 1);
    for (unsigned n = 0; n < SLOTS; n++, i = (i + 1) & (SLOTS - 1))
        if (!g_tab[i].k || !strcmp(g_tab[i].k, k)) return &g_tab[i];
    return NULL;
}

static const char *builtin_of(const char *k)
{
    for (int i = 0; i < EN_COUNT; i++) if (!strcmp(EN[i].k, k)) return EN[i].v;
    return NULL;
}

static void table_reset(void)
{
    for (int i = 0; i < SLOTS; i++) { free(g_tab[i].v); g_tab[i].v = NULL; g_tab[i].k = NULL; }
    g_rejected = 0;
}

/* Accepts one translated pair, unless its format specifiers disagree with English.
   The slot keeps a pointer to the *static* key from EN[], never to the parser's
   scratch buffer, so the table stays valid after the file is freed. */
static void accept_pair(void *ud, const char *k, const char *v)
{
    char a[64], b[64];
    const char *static_key = NULL, *en = NULL;
    size_t n;
    char *copy;
    Slot *s;
    (void)ud;

    for (int i = 0; i < EN_COUNT; i++)
        if (!strcmp(EN[i].k, k)) { static_key = EN[i].k; en = EN[i].v; break; }
    if (!static_key) return;                             /* key we do not know: ignore */

    if (strcmp(i18n_fmt_sig(en, a, sizeof a), i18n_fmt_sig(v, b, sizeof b))) { g_rejected++; return; }
    if (!(s = slot_of(static_key))) return;

    n = strlen(v) + 1;
    if (!(copy = (char *)malloc(n))) return;
    memcpy(copy, v, n);
    free(s->v);
    s->k = static_key;
    s->v = copy;
}

const char *T(const char *key)
{
    Slot *s = slot_of(key);
    const char *en;
    if (s && s->k && s->v) return s->v;
    if ((en = builtin_of(key)) != NULL) return en;
    return key;                                          /* never NULL */
}

int i18n_rejected(void) { return g_rejected; }
const char *i18n_current(void) { return g_lang; }

/* ------------------------------------------------------------------ files */

#define MAX_LANGS 32
static char g_dir[1024];
static struct { char code[16], name[64]; } g_langs[MAX_LANGS];
static int  g_nlangs;

int         i18n_count(void)       { return g_nlangs; }
const char *i18n_code(int i)       { return (i >= 0 && i < g_nlangs) ? g_langs[i].code : ""; }
const char *i18n_name(int i)       { return (i >= 0 && i < g_nlangs) ? g_langs[i].name : ""; }

/* fopen() on Windows takes a path in the local code page, so a UTF-8 path with
   non-ASCII characters (a Korean user name, say) would fail. Convert and use _wfopen. */
static FILE *open_utf8(const char *path)
{
#ifdef _WIN32
    wchar_t wpath[1200];
    int n = MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, (int)(sizeof wpath / sizeof wpath[0]));
    if (n <= 0) return NULL;
    return _wfopen(wpath, L"rb");
#else
    return fopen(path, "rb");
#endif
}

static char *read_whole(const char *path)
{
    long n;
    char *buf;
    size_t got;
    FILE *f = open_utf8(path);
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    n = ftell(f);
    if (n < 0 || n > (1 << 22)) { fclose(f); return NULL; }
    rewind(f);
    buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = 0;
    return buf;
}

static void grab_name(void *ud, const char *k, const char *v)
{
    char *out = (char *)ud;
    if (!strcmp(k, "_name")) { strncpy(out, v, 63); out[63] = 0; }
}

static void add_lang(const char *code, const char *name)
{
    for (int i = 0; i < g_nlangs; i++) if (!strcmp(g_langs[i].code, code)) return;
    if (g_nlangs >= MAX_LANGS) return;
    strncpy(g_langs[g_nlangs].code, code, 15);
    g_langs[g_nlangs].code[15] = 0;
    strncpy(g_langs[g_nlangs].name, (name && *name) ? name : code, 63);
    g_langs[g_nlangs].name[63] = 0;
    g_nlangs++;
}

bool i18n_set(const char *code)
{
    char path[1200], name[64] = "";
    char *txt;
    if (!code || !*code) return false;
    table_reset();
    if (!strcmp(code, "en")) { strncpy(g_lang, "en", sizeof g_lang - 1); return true; }
    snprintf(path, sizeof path, "%s%s.json", g_dir, code);
    if (!(txt = read_whole(path))) return false;
    if (!i18n_parse(txt, accept_pair, NULL)) { free(txt); table_reset(); return false; }
    i18n_parse(txt, grab_name, name);
    free(txt);
    strncpy(g_lang, code, sizeof g_lang - 1);
    g_lang[sizeof g_lang - 1] = 0;
    return true;
}

/* Registers lang/<code>.json. The caller may list the directory itself (SDL does
   that portably), which keeps i18n.c free of any SDL dependency. */
void i18n_add_file(const char *code)
{
    char path[1200], name[64] = "";
    char *txt;
    snprintf(path, sizeof path, "%s%s.json", g_dir, code);
    if (!(txt = read_whole(path))) return;
    i18n_parse(txt, grab_name, name);
    free(txt);
    add_lang(code, name);
}

void i18n_init(const char *lang_dir, const char *lang)
{
    static const char *const known[] = { "ko", "ja", "zh", "de", "fr", "es", "pt", "ru", "pl", "tr", "it", "nl", "uk", "vi", "th", "id" };
    size_t n;
    g_nlangs = 0;
    g_dir[0] = 0;
    if (lang_dir && *lang_dir) {
        strncpy(g_dir, lang_dir, sizeof g_dir - 2);
        g_dir[sizeof g_dir - 2] = 0;
        n = strlen(g_dir);
        if (n && g_dir[n - 1] != '/' && g_dir[n - 1] != '\\') { g_dir[n] = '/'; g_dir[n + 1] = 0; }
    }
    add_lang("en", "English");
    for (size_t i = 0; i < sizeof known / sizeof known[0]; i++) i18n_add_file(known[i]);
    if (lang && *lang && i18n_set(lang)) return;
    i18n_set("en");
}
