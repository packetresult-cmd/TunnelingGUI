/* i18n.h - JSON-backed UI strings for the GUI.
 *
 * Every user-visible string in the GUI goes through T("key").
 * English is compiled in, so the program works even with no lang/ folder at all;
 * lang/<code>.json files override it and can be edited or added by anyone.
 *
 * A translation is only used when its printf format specifiers match the built-in
 * English exactly (same specifiers, same order). A mismatched entry is ignored and
 * the English text is used instead, so a bad translation cannot crash the program.
 */
#ifndef TGUI_GUI_I18N_H
#define TGUI_GUI_I18N_H

#include <stdbool.h>
#include <stddef.h>

/* Translated text for `key`. Never returns NULL: falls back to built-in English,
   and to the key itself if the key is unknown. */
const char *T(const char *key);

/* Scan `lang_dir` for <code>.json files and select a language.
   `lang` may be a code ("en", "ko"), or NULL to auto-detect from the OS locale.
   Safe to call with a directory that does not exist. */
void i18n_init(const char *lang_dir, const char *lang);

/* Switch language at runtime. Returns false if that language is not available. */
bool i18n_set(const char *code);

const char *i18n_current(void);          /* "en", "ko", ... */

/* Register one more lang/<code>.json that i18n_init() did not find on its own.
   Adding a new language therefore needs no C changes: drop in the file. */
void i18n_add_file(const char *code);

/* Languages found on disk, plus the built-in "en". For a language picker. */
int         i18n_count(void);
const char *i18n_code(int i);            /* "ko" */
const char *i18n_name(int i);            /* "한국어" - the file's "_name" value */

/* Number of entries in the last loaded file that were rejected for a format
   specifier mismatch. Shown once at startup so translators notice. */
int         i18n_rejected(void);

/* --- exposed for tests ------------------------------------------------- */

/* Extract the printf specifier signature of `s` into `out` (e.g. "%s%d").
   "%%" is ignored. Returns `out`. */
char       *i18n_fmt_sig(const char *s, char *out, size_t sz);

/* Parse a flat JSON object of string->string. Calls `cb` for each pair.
   Returns false on malformed input. */
bool        i18n_parse(const char *json, void (*cb)(void *ud, const char *k, const char *v), void *ud);

/* The built-in English table, for tooling and tests. */
int         i18n_builtin_count(void);
const char *i18n_builtin_key(int i);
const char *i18n_builtin_value(int i);

#endif
