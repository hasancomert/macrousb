/**
 * macro.c -- macro script engine (see macro.h for the grammar).
 *
 * Single-threaded cooperative interpreter: macro_task() advances at most one
 * HID event per call, gated by tud_hid_ready(), so it never blocks the USB or
 * lwIP service loop. The device is "blind" (no screen feedback), so timing
 * between launch and the following actions relies on explicit `wait` steps.
 */
#include "macro.h"
#include "macro_set.h"
#include "board.h"
#include "keymap.h"
#include "usb_reports.h"

#include "tusb.h"
#include "class/hid/hid.h"

#include <string.h>

#define OPENER_DELAY_MS  700u    /* after the run-dialog chord, before typing */
#define INTERVAL_MIN_MS  20u

//--------------------------------------------------------------------+
// HID output helpers
//--------------------------------------------------------------------+
static void hid_key(uint8_t modifier, uint8_t keycode)
{
  uint8_t kc[6] = { keycode, 0, 0, 0, 0, 0 };
  tud_hid_keyboard_report(REPORT_ID_KEYBOARD, modifier, kc);
}
static void hid_key_release(void)
{
  uint8_t kc[6] = { 0 };
  tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, kc);
}
static void hid_mouse(uint8_t buttons, int8_t x, int8_t y)
{
  tud_hid_mouse_report(REPORT_ID_MOUSE, buttons, x, y, 0, 0);
}

//--------------------------------------------------------------------+
// action model
//--------------------------------------------------------------------+
typedef enum {
  A_TEXT, A_KEY, A_COMBO, A_WAIT, A_MOUSE_MOVE, A_MOUSE_CLICK, A_LAUNCH
} atype_t;

typedef struct {
  atype_t     type;
  const char *arg;      /* into s_run (text / launch target) */
  uint16_t    arglen;
  uint8_t     modifier;
  uint8_t     keycode;
  uint8_t     button;
  int16_t     dx, dy;
  uint32_t    ms;
} action_t;

//--------------------------------------------------------------------+
// engine state
//--------------------------------------------------------------------+
static char     s_run[MACRO_SCRIPT_MAX];
static uint16_t s_run_len;
static uint16_t s_pos;            /* parse cursor */
static uint16_t s_loop_point;     /* restart offset for `loop` (0 = whole script) */

static bool     s_running;
static bool     s_loop;
static uint32_t s_interval;
static host_profile_t s_host;

static bool     s_between;        /* waiting interval between loop runs */
static uint32_t s_next;

static action_t s_act;
static bool     s_have_act;
static bool     s_waiting;
static uint32_t s_wait_until;

/* per-action sub-state */
static uint16_t s_i;              /* text/launch char index */
static bool     s_press;          /* key down/up phase */
static uint8_t  s_phase;          /* launch phases */
static int16_t  s_mx, s_my;       /* remaining mouse move */

static uint8_t  s_release;        /* 2 = release keyboard next, 1 = release mouse next */

//--------------------------------------------------------------------+
// parsing helpers
//--------------------------------------------------------------------+
static bool itok_eq(const char *tok, size_t len, const char *name)
{
  size_t n = strlen(name);
  if (n != len) return false;
  for (size_t i = 0; i < len; i++) {
    char a = tok[i], b = name[i];
    if (a >= 'a' && a <= 'z') a = (char)(a - 32);
    if (b >= 'a' && b <= 'z') b = (char)(b - 32);
    if (a != b) return false;
  }
  return true;
}

/* Parse an unsigned integer, saturating at 1 hour so a `wait` value can never
 * overflow uint32 or break the signed-difference timer comparison. */
#define PARSE_U32_MAX 3600000u
static uint32_t parse_u32(const char *s, size_t len)
{
  uint32_t v = 0;
  for (size_t i = 0; i < len; i++) {
    if (s[i] < '0' || s[i] > '9') break;
    v = v * 10 + (uint32_t)(s[i] - '0');
    if (v > PARSE_U32_MAX) { v = PARSE_U32_MAX; }
  }
  return v;
}

static int16_t parse_i16(const char *s, size_t len)
{
  bool neg = false; size_t i = 0;
  if (i < len && (s[i] == '-' || s[i] == '+')) { neg = (s[i] == '-'); i++; }
  int32_t v = 0;
  for (; i < len; i++) {
    if (s[i] < '0' || s[i] > '9') break;
    v = v * 10 + (s[i] - '0');
  }
  if (neg) v = -v;
  if (v >  32767) v =  32767;
  if (v < -32768) v = -32768;
  return (int16_t)v;
}

/* Map a key name (or single character) to a HID modifier + keycode. */
static bool key_name(const char *s, size_t len, uint8_t *modifier, uint8_t *keycode)
{
  static const struct { const char *n; uint8_t k; } tbl[] = {
    {"ENTER", HID_KEY_ENTER}, {"RETURN", HID_KEY_ENTER}, {"TAB", HID_KEY_TAB},
    {"ESC", HID_KEY_ESCAPE}, {"ESCAPE", HID_KEY_ESCAPE}, {"SPACE", HID_KEY_SPACE},
    {"BACKSPACE", HID_KEY_BACKSPACE}, {"BKSP", HID_KEY_BACKSPACE},
    {"DELETE", HID_KEY_DELETE}, {"DEL", HID_KEY_DELETE},
    {"INSERT", HID_KEY_INSERT}, {"INS", HID_KEY_INSERT},
    {"HOME", HID_KEY_HOME}, {"END", HID_KEY_END},
    {"PAGEUP", HID_KEY_PAGE_UP}, {"PGUP", HID_KEY_PAGE_UP},
    {"PAGEDOWN", HID_KEY_PAGE_DOWN}, {"PGDN", HID_KEY_PAGE_DOWN},
    {"UP", HID_KEY_ARROW_UP}, {"DOWN", HID_KEY_ARROW_DOWN},
    {"LEFT", HID_KEY_ARROW_LEFT}, {"RIGHT", HID_KEY_ARROW_RIGHT},
    {"CAPSLOCK", HID_KEY_CAPS_LOCK},
  };
  for (unsigned i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++) {
    if (itok_eq(s, len, tbl[i].n)) { *modifier = 0; *keycode = tbl[i].k; return true; }
  }
  /* F1..F12 */
  if (len >= 2 && len <= 3 && (s[0] == 'F' || s[0] == 'f')) {
    uint32_t n = parse_u32(s + 1, len - 1);
    if (n >= 1 && n <= 12) { *modifier = 0; *keycode = (uint8_t)(HID_KEY_F1 + (n - 1)); return true; }
  }
  /* single printable character (keeps Shift for uppercase/shifted symbols) */
  if (len == 1) {
    uint8_t m, k;
    if (keymap_ascii_to_hid(s[0], &m, &k)) { *modifier = m; *keycode = k; return true; }
  }
  return false;
}

static uint8_t mod_name(const char *s, size_t len)
{
  if (itok_eq(s, len, "CTRL") || itok_eq(s, len, "CONTROL")) return KEYBOARD_MODIFIER_LEFTCTRL;
  if (itok_eq(s, len, "ALT")) return KEYBOARD_MODIFIER_LEFTALT;
  if (itok_eq(s, len, "SHIFT")) return KEYBOARD_MODIFIER_LEFTSHIFT;
  if (itok_eq(s, len, "GUI") || itok_eq(s, len, "WIN") || itok_eq(s, len, "SUPER") ||
      itok_eq(s, len, "META") || itok_eq(s, len, "CMD")) return KEYBOARD_MODIFIER_LEFTGUI;
  return 0;
}

/* Parse "CTRL+SHIFT+C" -> modifier bits + final keycode. */
static bool parse_combo(const char *s, size_t len, uint8_t *mod, uint8_t *key)
{
  uint8_t m = 0;
  size_t start = 0;
  for (size_t i = 0; i <= len; i++) {
    if (i == len || s[i] == '+') {
      const char *tok = s + start;
      size_t tlen = i - start;
      if (i == len) {                 /* final token = the key */
        uint8_t km, k;
        if (!key_name(tok, tlen, &km, &k)) return false;
        *key = k; *mod = (uint8_t)(m | km);
        return true;
      } else {
        uint8_t mm = mod_name(tok, tlen);
        if (mm == 0) return false;
        m |= mm;
      }
      start = i + 1;
    }
  }
  return false;
}

/* Read one line [out,outlen) from s_run, advancing s_pos past the newline. */
static bool next_line(const char **out, size_t *outlen)
{
  if (s_pos >= s_run_len) return false;
  const char *start = s_run + s_pos;
  size_t i = s_pos;
  while (i < s_run_len && s_run[i] != '\n') i++;
  size_t len = i - s_pos;
  if (len > 0 && start[len - 1] == '\r') len--;
  s_pos = (i < s_run_len) ? (uint16_t)(i + 1) : (uint16_t)i;
  *out = start; *outlen = len;
  return true;
}

static bool parse_mouse(const char *rest, size_t rlen)
{
  const char *t = rest; size_t tl = 0;
  while (tl < rlen && rest[tl] != ' ') tl++;
  const char *a = rest + tl; size_t al = rlen - tl;
  while (al && *a == ' ') { a++; al--; }

  if (itok_eq(t, tl, "move")) {
    const char *x = a; size_t xl = 0;
    while (xl < al && a[xl] != ' ') xl++;
    const char *y = a + xl; size_t yl = al - xl;
    while (yl && *y == ' ') { y++; yl--; }
    s_act.type = A_MOUSE_MOVE;
    s_act.dx = parse_i16(x, xl);
    s_act.dy = parse_i16(y, yl);
    return true;
  }
  if (itok_eq(t, tl, "click")) {
    uint8_t b = MOUSE_BUTTON_LEFT;
    if (itok_eq(a, al, "right"))  b = MOUSE_BUTTON_RIGHT;
    if (itok_eq(a, al, "middle")) b = MOUSE_BUTTON_MIDDLE;
    s_act.type = A_MOUSE_CLICK;
    s_act.button = b;
    return true;
  }
  return false;
}

/* Parse the next executable action into s_act. Returns false at end of script. */
static bool parse_next_action(void)
{
  const char *ln; size_t len;
  while (next_line(&ln, &len)) {
    while (len && (*ln == ' ' || *ln == '\t')) { ln++; len--; }
    if (len == 0 || *ln == '#') continue;

    const char *cmd = ln; size_t clen = 0;
    while (clen < len && ln[clen] != ' ' && ln[clen] != '\t') clen++;
    const char *rest = ln + clen; size_t rlen = len - clen;
    while (rlen && (*rest == ' ' || *rest == '\t')) { rest++; rlen--; }

    if (itok_eq(cmd, clen, "loop")) {
      /* marker: lines above run once; from here on repeats each loop.
       * s_pos is already past this line. */
      s_loop_point = s_pos;
      continue;
    }
    if (itok_eq(cmd, clen, "text")) {
      s_act.type = A_TEXT; s_act.arg = rest; s_act.arglen = (uint16_t)rlen; return true;
    }
    if (itok_eq(cmd, clen, "wait")) {
      s_act.type = A_WAIT; s_act.ms = parse_u32(rest, rlen); return true;
    }
    if (itok_eq(cmd, clen, "key")) {
      uint8_t m, k;
      if (key_name(rest, rlen, &m, &k)) { s_act.type = A_KEY; s_act.modifier = m; s_act.keycode = k; return true; }
      continue;
    }
    if (itok_eq(cmd, clen, "combo")) {
      uint8_t m, k;
      if (parse_combo(rest, rlen, &m, &k)) { s_act.type = A_COMBO; s_act.modifier = m; s_act.keycode = k; return true; }
      continue;
    }
    if (itok_eq(cmd, clen, "mouse")) {
      if (parse_mouse(rest, rlen)) return true;
      continue;
    }
    if (itok_eq(cmd, clen, "launch")) {
      /* skip the url/app token, keep the remainder as target */
      size_t kl = 0;
      while (kl < rlen && rest[kl] != ' ') kl++;
      const char *tgt = rest + kl; size_t tl = rlen - kl;
      while (tl && *tgt == ' ') { tgt++; tl--; }
      s_act.type = A_LAUNCH; s_act.arg = tgt; s_act.arglen = (uint16_t)tl; return true;
    }
    /* unknown command: skip */
  }
  return false;
}

//--------------------------------------------------------------------+
// execution
//--------------------------------------------------------------------+
static void opener_chord(uint8_t *mod, uint8_t *key)
{
  switch (s_host) {
    case HOST_KDE:   *mod = KEYBOARD_MODIFIER_LEFTALT; *key = HID_KEY_F2; break;
    case HOST_GNOME: *mod = KEYBOARD_MODIFIER_LEFTGUI; *key = 0;          break;  /* tap Super */
    case HOST_WIN:
    default:         *mod = KEYBOARD_MODIFIER_LEFTGUI; *key = HID_KEY_R;  break;
  }
}

static int8_t clamp8(int16_t v)
{
  if (v >  127) return  127;
  if (v < -127) return -127;
  return (int8_t)v;
}

static void action_begin(void)
{
  s_i = 0; s_press = true; s_phase = 0;
  if (s_act.type == A_MOUSE_MOVE) { s_mx = s_act.dx; s_my = s_act.dy; }
}

/* Execute one step of the current action. */
static void action_step(void)
{
  switch (s_act.type) {
    case A_WAIT:
      s_wait_until = board_millis() + s_act.ms;
      s_waiting = true;
      s_have_act = false;
      break;

    case A_TEXT:
      if (!tud_hid_ready()) return;
      if (s_press) {
        if (s_i >= s_act.arglen) { s_have_act = false; break; }
        uint8_t m, k;
        if (keymap_ascii_to_hid(s_act.arg[s_i], &m, &k)) { hid_key(m, k); s_press = false; }
        else s_i++;
      } else { hid_key_release(); s_press = true; s_i++; }
      break;

    case A_KEY:
    case A_COMBO:
      if (!tud_hid_ready()) return;
      if (s_press) { hid_key(s_act.modifier, s_act.keycode); s_press = false; }
      else         { hid_key_release(); s_have_act = false; }
      break;

    case A_MOUSE_MOVE:
      if (!tud_hid_ready()) return;
      if (s_mx == 0 && s_my == 0) { s_have_act = false; break; }
      {
        int8_t sx = clamp8(s_mx), sy = clamp8(s_my);
        hid_mouse(0, sx, sy);
        s_mx = (int16_t)(s_mx - sx);
        s_my = (int16_t)(s_my - sy);
        if (s_mx == 0 && s_my == 0) s_have_act = false;
      }
      break;

    case A_MOUSE_CLICK:
      if (!tud_hid_ready()) return;
      if (s_press) { hid_mouse(s_act.button, 0, 0); s_press = false; }
      else         { hid_mouse(0, 0, 0); s_have_act = false; }
      break;

    case A_LAUNCH:
      switch (s_phase) {
        case 0:  /* opener chord down */
          if (!tud_hid_ready()) return;
          { uint8_t m, k; opener_chord(&m, &k); hid_key(m, k); }
          s_phase = 1;
          break;
        case 1:  /* opener up, then wait for the dialog */
          if (!tud_hid_ready()) return;
          hid_key_release();
          s_wait_until = board_millis() + OPENER_DELAY_MS;
          s_waiting = true;
          s_i = 0; s_press = true; s_phase = 2;
          break;
        case 2:  /* type the target */
          if (!tud_hid_ready()) return;
          if (s_press) {
            if (s_i >= s_act.arglen) { s_phase = 3; break; }
            uint8_t m, k;
            if (keymap_ascii_to_hid(s_act.arg[s_i], &m, &k)) { hid_key(m, k); s_press = false; }
            else s_i++;
          } else { hid_key_release(); s_press = true; s_i++; }
          break;
        case 3:  /* Enter down */
          if (!tud_hid_ready()) return;
          hid_key(0, HID_KEY_ENTER); s_phase = 4;
          break;
        case 4:  /* Enter up */
          if (!tud_hid_ready()) return;
          hid_key_release(); s_have_act = false;
          break;
        default: s_have_act = false; break;
      }
      break;
  }
}

//--------------------------------------------------------------------+
// public API
//--------------------------------------------------------------------+
void macro_start(const char *script, bool loop, uint32_t interval_ms, host_profile_t host)
{
  size_t n = strnlen(script, MACRO_SCRIPT_MAX - 1);
  memcpy(s_run, script, n);
  s_run[n] = '\0';
  s_run_len = (uint16_t)n;
  s_pos = 0;
  s_loop_point = 0;

  s_loop = loop;
  s_interval = (interval_ms < INTERVAL_MIN_MS) ? INTERVAL_MIN_MS : interval_ms;
  s_host = host;

  s_running = true;
  s_between = false;
  s_have_act = false;
  s_waiting = false;
  s_release = 0;
}

void macro_stop(void)
{
  s_running = false;
  s_have_act = false;
  s_waiting = false;
  s_between = false;
  s_release = 2;   /* release held keys, then mouse (separate HID reports) */
}

bool macro_running(void) { return s_running; }

bool macro_is_emitting(void)
{
  return s_running && !s_between && !s_waiting && s_have_act;
}

void macro_task(void)
{
  /* Release held inputs across two HID-ready cycles: keyboard and mouse share
   * one endpoint, so they cannot be sent back-to-back. */
  if (s_release && tud_mounted() && tud_hid_ready()) {
    if (s_release == 2) { hid_key_release(); s_release = 1; }
    else                { hid_mouse(0, 0, 0); s_release = 0; }
  }

  if (!s_running || !tud_mounted()) return;

  uint32_t now = board_millis();

  if (s_between) {
    if ((int32_t)(now - s_next) < 0) return;
    s_between = false;
    s_pos = s_loop_point;      /* restart (after `loop` marker, or top) */
  }

  if (s_waiting) {
    if ((int32_t)(now - s_wait_until) < 0) return;
    s_waiting = false;
  }

  if (!s_have_act) {
    if (!parse_next_action()) {       /* end of script */
      if (s_loop) { s_next = now + s_interval; s_between = true; }
      else        { macro_stop(); }
      return;
    }
    s_have_act = true;
    action_begin();
  }

  action_step();
}
