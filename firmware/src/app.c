/**
 * app.c -- macro typing application.
 *
 * Holds the configuration (text, interval, "press Enter"), the running state,
 * the HID emission state machine, the HTTP /api endpoints and the status LED.
 *
 * Everything runs in the single cooperative main-loop thread: app_http_api()
 * is invoked from within tud_task() (USB -> lwIP -> TCP -> HTTP), the same
 * context as app_task(), so no locking is needed.
 */
#include "app.h"
#include "board.h"
#include "keymap.h"
#include "http_server.h"

#include "tusb.h"
#include "class/hid/hid.h"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#define TEXT_MAX        128u
#define INTERVAL_MIN_MS 50u
#define INTERVAL_MAX_MS 3600000u   /* 1 hour */

//--------------------------------------------------------------------+
// configuration + state
//--------------------------------------------------------------------+
static char     s_text[TEXT_MAX] = "";
static uint32_t s_interval_ms = 5000;
static bool     s_enter = false;

static bool     s_running = false;
static uint32_t s_count = 0;
static uint32_t s_next_ms = 0;

/* HID emission sub-state: a virtual sequence of s_seq_len "key items"
 * (text characters, then optionally Enter); each item = key-down then key-up. */
static bool     s_emitting = false;
static size_t   s_seq_len = 0;
static size_t   s_emit_i = 0;
static bool     s_emit_press = true;        /* next action: true=down, false=up */
static bool     s_release_pending = false;  /* flush an all-keys-up report       */

//--------------------------------------------------------------------+
// HID helpers
//--------------------------------------------------------------------+
static void hid_send_key(uint8_t modifier, uint8_t keycode)
{
  uint8_t kc[6] = { keycode, 0, 0, 0, 0, 0 };
  tud_hid_keyboard_report(0, modifier, kc);
}

static void hid_send_release(void)
{
  uint8_t kc[6] = { 0, 0, 0, 0, 0, 0 };
  tud_hid_keyboard_report(0, 0, kc);
}

//--------------------------------------------------------------------+
// typing state machine
//--------------------------------------------------------------------+
static void emission_begin(void)
{
  s_emit_i = 0;
  s_emit_press = true;
  s_seq_len = strlen(s_text) + (s_enter ? 1u : 0u);
  s_emitting = (s_seq_len > 0);
  if (!s_emitting) {           /* nothing to type: still count the "burst" */
    s_count++;
    s_next_ms = board_millis() + s_interval_ms;
  }
}

/* Called only when s_emitting && tud_hid_ready(). */
static void emission_step(void)
{
  size_t text_len = strlen(s_text);

  if (s_emit_press) {
    if (s_emit_i >= s_seq_len) {               /* finished the whole sequence */
      s_emitting = false;
      s_count++;
      s_next_ms = board_millis() + s_interval_ms;
      return;
    }
    uint8_t mod, key;
    bool ok;
    if (s_emit_i < text_len) {
      ok = keymap_ascii_to_hid(s_text[s_emit_i], &mod, &key);
    } else {                                   /* the trailing Enter item */
      mod = 0; key = HID_KEY_ENTER; ok = true;
    }
    if (!ok) { s_emit_i++; return; }           /* skip untypable char */
    hid_send_key(mod, key);
    s_emit_press = false;
  } else {
    hid_send_release();
    s_emit_press = true;
    s_emit_i++;
  }
}

static void app_start(void)
{
  s_running   = true;
  s_count     = 0;
  s_emitting  = false;
  s_emit_press = true;
  /* First burst after one interval so there is time to focus the target
   * window after pressing Start in the browser. */
  s_next_ms = board_millis() + s_interval_ms;
}

static void app_stop(void)
{
  s_running  = false;
  s_emitting = false;
  s_release_pending = true;   /* make sure no key stays logically held */
}

//--------------------------------------------------------------------+
// small parsing helpers
//--------------------------------------------------------------------+
static int hexval(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static void url_decode(const char *src, char *dst, size_t cap)
{
  size_t j = 0;
  for (size_t i = 0; src[i] && j + 1 < cap; i++) {
    char c = src[i];
    if (c == '+') {
      dst[j++] = ' ';
    } else if (c == '%' && hexval(src[i + 1]) >= 0 && hexval(src[i + 2]) >= 0) {
      dst[j++] = (char)((hexval(src[i + 1]) << 4) | hexval(src[i + 2]));
      i += 2;
    } else {
      dst[j++] = c;
    }
  }
  dst[j] = '\0';
}

/* Find key in "a=1&b=2" query; URL-decode its value into out. */
static bool query_get(const char *query, const char *key, char *out, size_t cap)
{
  if (!query) return false;
  size_t klen = strlen(key);
  const char *p = query;
  while (p && *p) {
    const char *amp = strchr(p, '&');
    const char *end = amp ? amp : (p + strlen(p));
    if ((size_t)(end - p) > klen && strncmp(p, key, klen) == 0 && p[klen] == '=') {
      char raw[512];   /* URL-encoded value (decoded result is shorter) */
      size_t vlen = (size_t)(end - (p + klen + 1));
      if (vlen > sizeof(raw) - 1) vlen = sizeof(raw) - 1;
      memcpy(raw, p + klen + 1, vlen);
      raw[vlen] = '\0';
      url_decode(raw, out, cap);
      return true;
    }
    if (!amp) break;
    p = amp + 1;
  }
  return false;
}

static uint32_t parse_seconds_ms(const char *s)
{
  uint32_t ip = 0, fp = 0;
  int fdig = 0; bool dot = false, any = false;
  for (; *s; s++) {
    char c = *s;
    if (c >= '0' && c <= '9') {
      any = true;
      if (!dot)            ip = ip * 10 + (uint32_t)(c - '0');
      else if (fdig < 3) { fp = fp * 10 + (uint32_t)(c - '0'); fdig++; }
    } else if (c == '.' || c == ',') {
      dot = true;
    } else {
      break;
    }
  }
  if (!any) return 0;
  while (fdig < 3) { fp *= 10; fdig++; }   /* scale fraction to milliseconds */
  return ip * 1000 + fp;
}

static void json_escape(const char *src, char *dst, size_t cap)
{
  size_t j = 0;
  for (size_t i = 0; src[i] && j + 2 < cap; i++) {
    char c = src[i];
    if (c == '"' || c == '\\') { dst[j++] = '\\'; dst[j++] = c; }
    else if ((unsigned char)c >= 0x20)        { dst[j++] = c; }
    /* else: drop control characters */
  }
  dst[j] = '\0';
}

//--------------------------------------------------------------------+
// HTTP API  (strong override of the weak default in http_server.c)
//--------------------------------------------------------------------+
static int status_json(char *out, int cap)
{
  char esc[2 * TEXT_MAX + 4];
  json_escape(s_text, esc, sizeof(esc));
  uint32_t sec = s_interval_ms / 1000;
  uint32_t tenths = (s_interval_ms % 1000) / 100;
  int n = snprintf(out, (size_t)cap,
    "{\"running\":%s,\"interval\":%lu.%lu,\"enter\":%s,\"count\":%lu,\"text\":\"%s\"}",
    s_running ? "true" : "false",
    (unsigned long)sec, (unsigned long)tenths,
    s_enter ? "true" : "false",
    (unsigned long)s_count, esc);
  if (n < 0) return 0;
  return (n < cap) ? n : (cap - 1);
}

static void apply_config(const char *query)
{
  char buf[TEXT_MAX + 32];
  if (query_get(query, "text", buf, sizeof(buf))) {
    strncpy(s_text, buf, TEXT_MAX - 1);
    s_text[TEXT_MAX - 1] = '\0';
  }
  if (query_get(query, "interval", buf, sizeof(buf))) {
    uint32_t ms = parse_seconds_ms(buf);
    if (ms < INTERVAL_MIN_MS) ms = INTERVAL_MIN_MS;
    if (ms > INTERVAL_MAX_MS) ms = INTERVAL_MAX_MS;
    s_interval_ms = ms;
  }
  if (query_get(query, "enter", buf, sizeof(buf))) {
    s_enter = (buf[0] == '1' || buf[0] == 't' || buf[0] == 'T');
  }
}

int app_http_api(const char *method, const char *path, const char *query,
                 char *body, int body_cap)
{
  (void)method;
  if (strcmp(path, "/api/status") == 0) {
    return status_json(body, body_cap);
  }
  if (strcmp(path, "/api/stop") == 0) {
    app_stop();
    return status_json(body, body_cap);
  }
  if (strcmp(path, "/api/config") == 0) {
    apply_config(query);
    return status_json(body, body_cap);
  }
  if (strcmp(path, "/api/start") == 0) {
    apply_config(query);
    app_start();
    return status_json(body, body_cap);
  }
  return -1;   /* unknown -> 404 */
}

//--------------------------------------------------------------------+
// LED status
//--------------------------------------------------------------------+
static void led_update(void)
{
  uint32_t now = board_millis();
  bool on;
  if (!tud_mounted())      on = (now / 120) & 1;        /* fast blink: no host */
  else if (!s_running)     on = (now % 2000) < 40;      /* idle heartbeat blip */
  else if (s_emitting)     on = (now / 60) & 1;         /* typing: flicker      */
  else                     on = true;                   /* running, waiting     */
  board_led_write(on);
}

//--------------------------------------------------------------------+
// public
//--------------------------------------------------------------------+
void app_init(void)
{
  /* Safe defaults; nothing types until Start from the web UI. */
  s_text[0] = '\0';
  s_interval_ms = 5000;
  s_enter = false;
  s_running = false;
}

void app_task(void)
{
  if (s_release_pending && tud_mounted() && tud_hid_ready()) {
    hid_send_release();
    s_release_pending = false;
  }

  if (s_running && tud_mounted()) {
    if (!s_emitting && (int32_t)(board_millis() - s_next_ms) >= 0) {
      emission_begin();
    }
    if (s_emitting && tud_hid_ready()) {
      emission_step();
    }
  }

  led_update();
}
