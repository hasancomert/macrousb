/**
 * app.c -- orchestration: holds the macro set, serves the HTTP /api, drives the
 * macro engine and the status LED. Runs in the single cooperative main loop.
 *
 * API (all GET; JSON responses):
 *   /api/status                         running/emitting/selected/loop/interval/host
 *   /api/list                           selected/host/loop/interval + macro names
 *   /api/macro?i=N                      one macro's name + script
 *   /api/setmacro?i=N&name=..&script=.. create/replace a macro (RAM)
 *   /api/add                            append a blank macro
 *   /api/delete?i=N                     remove a macro
 *   /api/select?i=N                     choose the active macro
 *   /api/opts?loop=0|1&interval=S&host=0|1|2
 *   /api/start   /api/stop              run / stop the selected macro
 *   /api/save                           persist the whole set to flash (USB pauses ~1-2 s)
 */
#include "app.h"
#include "board.h"
#include "http_server.h"
#include "flash_store.h"
#include "macro.h"
#include "macro_set.h"

#include "tusb.h"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#define INTERVAL_MIN_MS 20u
#define INTERVAL_MAX_MS 3600000u

static macro_set_t g_set;

//--------------------------------------------------------------------+
// defaults
//--------------------------------------------------------------------+
static void set_defaults(void)
{
  memset(&g_set, 0, sizeof(g_set));
  g_set.count = 1;
  g_set.selected = 0;
  g_set.host = HOST_WIN;
  g_set.loop = 0;
  g_set.interval_ms = 3000;
  strncpy(g_set.items[0].name, "Ornek", MACRO_NAME_MAX - 1);
  strncpy(g_set.items[0].script,
          "# ornek makro\n"
          "text merhaba\n"
          "key ENTER\n",
          MACRO_SCRIPT_MAX - 1);
}

//--------------------------------------------------------------------+
// parsing helpers
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

static bool query_get(const char *query, const char *key, char *out, size_t cap)
{
  if (!query) return false;
  size_t klen = strlen(key);
  const char *p = query;
  while (p && *p) {
    const char *amp = strchr(p, '&');
    const char *end = amp ? amp : (p + strlen(p));
    if ((size_t)(end - p) > klen && strncmp(p, key, klen) == 0 && p[klen] == '=') {
      char raw[MACRO_SCRIPT_MAX * 3];      /* URL-encoded value */
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

static bool query_get_int(const char *query, const char *key, long *out)
{
  char buf[16];
  if (!query_get(query, key, buf, sizeof(buf))) return false;
  long v = 0; size_t i = 0; bool neg = false;
  if (buf[i] == '-') { neg = true; i++; }
  bool any = false;
  for (; buf[i] >= '0' && buf[i] <= '9'; i++) { v = v * 10 + (buf[i] - '0'); any = true; }
  if (!any) return false;
  *out = neg ? -v : v;
  return true;
}

static uint32_t parse_seconds_ms(const char *s)
{
  uint32_t ip = 0, fp = 0; int fdig = 0; bool dot = false, any = false;
  for (; *s; s++) {
    char c = *s;
    if (c >= '0' && c <= '9') {
      any = true;
      if (!dot)            ip = ip * 10 + (uint32_t)(c - '0');
      else if (fdig < 3) { fp = fp * 10 + (uint32_t)(c - '0'); fdig++; }
    } else if (c == '.' || c == ',') dot = true;
    else break;
  }
  if (!any) return 0;
  while (fdig < 3) { fp *= 10; fdig++; }
  return ip * 1000 + fp;
}

static void json_escape(const char *src, char *dst, size_t cap)
{
  size_t j = 0;
  for (size_t i = 0; src[i] && j + 2 < cap; i++) {
    char c = src[i];
    switch (c) {
      case '"':  dst[j++] = '\\'; dst[j++] = '"';  break;
      case '\\': dst[j++] = '\\'; dst[j++] = '\\'; break;
      case '\n': dst[j++] = '\\'; dst[j++] = 'n';  break;
      case '\t': dst[j++] = '\\'; dst[j++] = 't';  break;
      case '\r': break;
      default:   if ((unsigned char)c >= 0x20) dst[j++] = c; break;
    }
  }
  dst[j] = '\0';
}

//--------------------------------------------------------------------+
// JSON responses
//--------------------------------------------------------------------+
static int fmt_interval(char *buf, size_t cap)
{
  uint32_t sec = g_set.interval_ms / 1000, tenths = (g_set.interval_ms % 1000) / 100;
  return snprintf(buf, cap, "%lu.%lu", (unsigned long)sec, (unsigned long)tenths);
}

static int status_json(char *out, int cap)
{
  char ivs[16]; fmt_interval(ivs, sizeof(ivs));
  char name[2 * MACRO_NAME_MAX];
  name[0] = '\0';
  if (g_set.count > 0 && g_set.selected < g_set.count)
    json_escape(g_set.items[g_set.selected].name, name, sizeof(name));
  int n = snprintf(out, (size_t)cap,
    "{\"running\":%s,\"emitting\":%s,\"loop\":%s,\"interval\":%s,\"host\":%u,"
    "\"selected\":%u,\"count\":%u,\"name\":\"%s\"}",
    macro_running() ? "true" : "false",
    macro_is_emitting() ? "true" : "false",
    g_set.loop ? "true" : "false",
    ivs, g_set.host, g_set.selected, g_set.count, name);
  return (n < 0) ? 0 : (n < cap ? n : cap - 1);
}

static int list_json(char *out, int cap)
{
  char ivs[16]; fmt_interval(ivs, sizeof(ivs));
  int n = snprintf(out, (size_t)cap,
    "{\"selected\":%u,\"host\":%u,\"loop\":%s,\"interval\":%s,\"count\":%u,\"names\":[",
    g_set.selected, g_set.host, g_set.loop ? "true" : "false", ivs, g_set.count);
  for (unsigned i = 0; i < g_set.count && n > 0 && n < cap; i++) {
    char nm[2 * MACRO_NAME_MAX];
    json_escape(g_set.items[i].name, nm, sizeof(nm));
    int m = snprintf(out + n, (size_t)(cap - n), "%s\"%s\"", (i ? "," : ""), nm);
    if (m < 0) break;
    n += m;
  }
  if (n >= 0 && n < cap) n += snprintf(out + n, (size_t)(cap - n), "]}");
  return (n < 0) ? 0 : (n < cap ? n : cap - 1);
}

static int macro_json(char *out, int cap, unsigned i)
{
  if (i >= g_set.count) return snprintf(out, (size_t)cap, "{\"i\":-1}");
  char nm[2 * MACRO_NAME_MAX];
  char sc[2 * MACRO_SCRIPT_MAX];
  json_escape(g_set.items[i].name, nm, sizeof(nm));
  json_escape(g_set.items[i].script, sc, sizeof(sc));
  int n = snprintf(out, (size_t)cap, "{\"i\":%u,\"name\":\"%s\",\"script\":\"%s\"}", i, nm, sc);
  return (n < 0) ? 0 : (n < cap ? n : cap - 1);
}

//--------------------------------------------------------------------+
// run control
//--------------------------------------------------------------------+
static void do_start(void)
{
  if (g_set.count == 0 || g_set.selected >= g_set.count) return;
  macro_start(g_set.items[g_set.selected].script, g_set.loop != 0,
              g_set.interval_ms, (host_profile_t)g_set.host);
}

//--------------------------------------------------------------------+
// HTTP API
//--------------------------------------------------------------------+
int app_http_api(const char *method, const char *path, const char *query,
                 char *body, int body_cap)
{
  (void)method;

  if (strcmp(path, "/api/status") == 0) return status_json(body, body_cap);
  if (strcmp(path, "/api/list")   == 0) return list_json(body, body_cap);

  if (strcmp(path, "/api/macro") == 0) {
    long i = 0; query_get_int(query, "i", &i);
    return macro_json(body, body_cap, (unsigned)(i < 0 ? 0 : i));
  }

  if (strcmp(path, "/api/setmacro") == 0) {
    long i = -1;
    if (query_get_int(query, "i", &i) && i >= 0 && i <= g_set.count && i < MACRO_COUNT_MAX) {
      if ((unsigned)i == g_set.count) {                /* append: clear the new slot */
        memset(&g_set.items[i], 0, sizeof(g_set.items[i]));
        g_set.count++;
      }
      char buf[MACRO_SCRIPT_MAX];
      if (query_get(query, "name", buf, sizeof(buf))) {
        strncpy(g_set.items[i].name, buf, MACRO_NAME_MAX - 1);
        g_set.items[i].name[MACRO_NAME_MAX - 1] = '\0';
      }
      if (query_get(query, "script", buf, sizeof(buf))) {
        strncpy(g_set.items[i].script, buf, MACRO_SCRIPT_MAX - 1);
        g_set.items[i].script[MACRO_SCRIPT_MAX - 1] = '\0';
      }
    }
    return list_json(body, body_cap);
  }

  if (strcmp(path, "/api/add") == 0) {
    if (g_set.count < MACRO_COUNT_MAX) {
      unsigned i = g_set.count++;
      memset(&g_set.items[i], 0, sizeof(g_set.items[i]));
      snprintf(g_set.items[i].name, MACRO_NAME_MAX, "Makro %u", i + 1);
      g_set.selected = i;
    }
    return list_json(body, body_cap);
  }

  if (strcmp(path, "/api/delete") == 0) {
    long i = -1;
    if (query_get_int(query, "i", &i) && i >= 0 && (unsigned)i < g_set.count) {
      for (unsigned j = (unsigned)i; j + 1 < g_set.count; j++) g_set.items[j] = g_set.items[j + 1];
      g_set.count--;
      if (g_set.selected >= g_set.count) g_set.selected = (g_set.count > 0) ? g_set.count - 1 : 0;
    }
    return list_json(body, body_cap);
  }

  if (strcmp(path, "/api/select") == 0) {
    long i = -1;
    if (query_get_int(query, "i", &i) && i >= 0 && (unsigned)i < g_set.count) g_set.selected = (uint8_t)i;
    return status_json(body, body_cap);
  }

  if (strcmp(path, "/api/opts") == 0) {
    long v;
    if (query_get_int(query, "loop", &v)) g_set.loop = (v != 0);
    if (query_get_int(query, "host", &v) && v >= 0 && v <= 2) g_set.host = (uint8_t)v;
    char buf[24];
    if (query_get(query, "interval", buf, sizeof(buf))) {
      uint32_t ms = parse_seconds_ms(buf);
      if (ms < INTERVAL_MIN_MS) ms = INTERVAL_MIN_MS;
      if (ms > INTERVAL_MAX_MS) ms = INTERVAL_MAX_MS;
      g_set.interval_ms = ms;
    }
    return status_json(body, body_cap);
  }

  if (strcmp(path, "/api/start") == 0) { do_start();   return status_json(body, body_cap); }
  if (strcmp(path, "/api/stop")  == 0) { macro_stop(); return status_json(body, body_cap); }
  if (strcmp(path, "/api/save")  == 0) { flash_store_save(&g_set); return status_json(body, body_cap); }

  return -1;
}

//--------------------------------------------------------------------+
// status LED
//--------------------------------------------------------------------+
static void led_update(void)
{
  uint32_t now = board_millis();
  bool on;
  if (!tud_mounted())           on = (now / 120) & 1;     /* fast blink: no host */
  else if (macro_is_emitting()) on = (now / 60) & 1;      /* typing: flicker     */
  else if (macro_running())     on = true;                /* running, waiting    */
  else                          on = (now % 2000) < 40;   /* idle heartbeat blip */
  board_led_write(on);
}

//--------------------------------------------------------------------+
// public
//--------------------------------------------------------------------+
void app_init(void)
{
  if (!flash_store_load(&g_set)) set_defaults();
  if (g_set.count > MACRO_COUNT_MAX) set_defaults();
  if (g_set.selected >= g_set.count) g_set.selected = 0;
}

void app_task(void)
{
  macro_task();
  led_update();
}
