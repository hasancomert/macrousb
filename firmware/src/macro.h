/**
 * macro.h -- macro script engine.
 *
 * Executes a small line-based script as USB HID keyboard + mouse actions,
 * cooperatively (one step per main-loop pass, gated by HID readiness).
 *
 * Script grammar (one action per line; '#' starts a comment):
 *   launch url  <target>     open <target> in the default browser
 *   launch app  <command>    run <command>
 *      (both expand to the host profile's "run" sequence, then <target>, Enter)
 *   text <literal...>        type literal text (US QWERTY)
 *   key <NAME>               press+release a key (ENTER TAB ESC LEFT RIGHT UP
 *                            DOWN HOME END PAGEUP PAGEDOWN DELETE BACKSPACE
 *                            SPACE INSERT F1..F12, or a single character)
 *   combo <MOD>+...+<KEY>     chord, e.g. CTRL+C, CTRL+SHIFT+T, ALT+F4
 *                            (mods: CTRL ALT SHIFT GUI/WIN/SUPER)
 *   wait <ms>                delay in milliseconds
 *   mouse move <dx> <dy>     relative move (pixels, can be negative)
 *   mouse click <left|right|middle>
 *   loop                     when looping: lines ABOVE run once, lines BELOW
 *                            repeat each loop (e.g. launch once, then repeat)
 */
#ifndef MACRO_H_
#define MACRO_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum {
  HOST_WIN   = 0,   /* Win+R   */
  HOST_KDE   = 1,   /* Alt+F2 (KRunner) */
  HOST_GNOME = 2,   /* tap Super */
} host_profile_t;

/* Begin executing `script` (copied internally). If loop, the script repeats
 * every interval_ms; otherwise it runs once and stops. */
void macro_start(const char *script, bool loop, uint32_t interval_ms, host_profile_t host);

/* Stop and release any held keys/buttons. */
void macro_stop(void);

/* Step the engine; call every main-loop pass. */
void macro_task(void);

bool macro_running(void);      /* started and not yet finished/stopped */
bool macro_is_emitting(void);  /* actively sending HID right now (for LED) */

#endif /* MACRO_H_ */
