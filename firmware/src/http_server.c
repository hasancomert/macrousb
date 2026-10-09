/**
 * http_server.c -- tiny HTTP/1.0 server on lwIP raw TCP (NO_SYS).
 *
 * Deliberately minimal: parses the request line only (GET, plus GET-style
 * "/api/..." calls with query strings). No POST body handling, no keep-alive.
 * Responses are sent with "Connection: close". Enough for a single-page UI.
 */
#include "http_server.h"
#include "web_content.h"

#include "lwip/tcp.h"
#include "lwip/pbuf.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define REQ_MAX   192
#define BUF_MAX   768
#define JSON_MAX  512

typedef struct {
  struct tcp_pcb *pcb;
  char      req[REQ_MAX];   /* accumulates the request line */
  uint16_t  req_len;
  bool      responded;
  char      buf[BUF_MAX];   /* composed status line + headers (+ JSON body) */
  const char *seg[2];       /* seg[0]=buf, seg[1]=optional static body */
  uint32_t  seglen[2];
  uint8_t   segidx;
  uint32_t  segoff;
} http_conn_t;

/* Weak default: no API endpoints yet (added in a later step). */
__attribute__((weak))
int app_http_api(const char *method, const char *path, const char *query,
                 char *body, int body_cap)
{
  (void)method; (void)path; (void)query; (void)body; (void)body_cap;
  return -1;
}

//--------------------------------------------------------------------+
// connection teardown
//--------------------------------------------------------------------+
static void http_close(http_conn_t *c)
{
  struct tcp_pcb *pcb = c->pcb;
  tcp_arg(pcb, NULL);
  tcp_recv(pcb, NULL);
  tcp_sent(pcb, NULL);
  tcp_err(pcb, NULL);
  tcp_poll(pcb, NULL, 0);
  mem_free(c);
  if (tcp_close(pcb) != ERR_OK) {
    tcp_abort(pcb);
  }
}

//--------------------------------------------------------------------+
// push queued response segments into the TCP send buffer
//--------------------------------------------------------------------+
static void http_flush(http_conn_t *c)
{
  struct tcp_pcb *pcb = c->pcb;

  while (c->segidx < 2) {
    uint32_t total = c->seglen[c->segidx];
    if (c->segoff >= total) { c->segidx++; c->segoff = 0; continue; }

    uint16_t sb = tcp_sndbuf(pcb);
    if (sb == 0) { tcp_output(pcb); return; }   /* wait for sent_cb */

    uint32_t rem = total - c->segoff;
    uint16_t n   = (rem < sb) ? (uint16_t)rem : sb;

    /* seg[0] (headers) lives in the conn struct -> copy; seg[1] is static
     * flash (the page) -> no copy. */
    u8_t flag = (c->segidx == 0) ? TCP_WRITE_FLAG_COPY : 0;
    bool more = (c->segoff + n < total) ||
                (c->segidx + 1 < 2 && c->seglen[c->segidx + 1] > 0);
    if (more) flag |= TCP_WRITE_FLAG_MORE;

    err_t e = tcp_write(pcb, c->seg[c->segidx] + c->segoff, n, flag);
    if (e == ERR_MEM) { tcp_output(pcb); return; }  /* retry in sent_cb */
    if (e != ERR_OK)  { http_close(c); return; }
    c->segoff += n;
  }

  tcp_output(pcb);
  http_close(c);   /* everything queued; headers copied, body is static */
}

//--------------------------------------------------------------------+
// build responses
//--------------------------------------------------------------------+
static void resp_page(http_conn_t *c)
{
  int n = snprintf(c->buf, sizeof(c->buf),
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Content-Length: %u\r\n"
    "Connection: close\r\n\r\n",
    (unsigned)INDEX_HTML_LEN);
  c->seg[0] = c->buf;        c->seglen[0] = (uint32_t)n;
  c->seg[1] = INDEX_HTML;    c->seglen[1] = INDEX_HTML_LEN;
}

static void resp_simple(http_conn_t *c, const char *status, const char *ctype,
                        const char *body)
{
  int n = snprintf(c->buf, sizeof(c->buf),
    "HTTP/1.0 %s\r\n"
    "Content-Type: %s\r\n"
    "Content-Length: %u\r\n"
    "Connection: close\r\n\r\n%s",
    status, ctype, (unsigned)strlen(body), body);
  c->seg[0] = c->buf; c->seglen[0] = (uint32_t)n;
  c->seg[1] = NULL;   c->seglen[1] = 0;
}

static void resp_json(http_conn_t *c, const char *json, int json_len)
{
  int n = snprintf(c->buf, sizeof(c->buf),
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: %d\r\n"
    "Cache-Control: no-store\r\n"
    "Connection: close\r\n\r\n%.*s",
    json_len, json_len, json);
  c->seg[0] = c->buf; c->seglen[0] = (uint32_t)n;
  c->seg[1] = NULL;   c->seglen[1] = 0;
}

//--------------------------------------------------------------------+
// route the (already-parsed) request line
//--------------------------------------------------------------------+
static void http_route(http_conn_t *c)
{
  /* Parse "METHOD SP PATH SP ..." from c->req */
  char *line = c->req;
  char *sp1  = strchr(line, ' ');
  if (!sp1) { resp_simple(c, "400 Bad Request", "text/plain", "bad request"); return; }
  *sp1 = '\0';
  const char *method = line;
  char *target = sp1 + 1;
  char *sp2 = strchr(target, ' ');
  if (sp2) *sp2 = '\0';

  /* split path?query */
  char *query = strchr(target, '?');
  if (query) { *query = '\0'; query++; }
  const char *path = target;

  if (strncmp(path, "/api/", 5) == 0) {
    char json[JSON_MAX];
    int jl = app_http_api(method, path, query, json, (int)sizeof(json));
    if (jl < 0) { resp_simple(c, "404 Not Found", "application/json", "{\"error\":\"not found\"}"); }
    else        { resp_json(c, json, jl); }
    return;
  }

  if (strcmp(method, "GET") != 0) {
    resp_simple(c, "405 Method Not Allowed", "text/plain", "method not allowed");
    return;
  }

  if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
    resp_page(c);
  } else {
    resp_simple(c, "404 Not Found", "text/plain", "not found");
  }
}

//--------------------------------------------------------------------+
// lwIP callbacks
//--------------------------------------------------------------------+
static err_t on_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
  (void)pcb; (void)len;
  http_conn_t *c = (http_conn_t *)arg;
  if (c) http_flush(c);
  return ERR_OK;
}

static err_t on_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
  http_conn_t *c = (http_conn_t *)arg;
  if (!c) { if (p) pbuf_free(p); return ERR_OK; }

  if (p == NULL) { http_close(c); return ERR_OK; }       /* peer closed */
  if (err != ERR_OK) { pbuf_free(p); return ERR_OK; }

  tcp_recved(pcb, p->tot_len);

  if (!c->responded) {
    uint16_t space = (uint16_t)(REQ_MAX - 1 - c->req_len);
    if (space > 0) {
      uint16_t n = pbuf_copy_partial(p, c->req + c->req_len, space, 0);
      c->req_len += n;
      c->req[c->req_len] = '\0';
    }
    char *eol = strstr(c->req, "\r\n");
    if (eol || c->req_len >= REQ_MAX - 1) {
      if (eol) *eol = '\0';
      c->responded = true;
      http_route(c);
      pbuf_free(p);
      http_flush(c);
      return ERR_OK;
    }
  }

  pbuf_free(p);
  return ERR_OK;
}

static void on_err(void *arg, err_t err)
{
  (void)err;
  http_conn_t *c = (http_conn_t *)arg;
  if (c) mem_free(c);   /* pcb already freed by lwIP */
}

static err_t on_poll(void *arg, struct tcp_pcb *pcb)
{
  http_conn_t *c = (http_conn_t *)arg;
  if (!c) { tcp_abort(pcb); return ERR_ABRT; }
  /* idle/stuck connection: drop it */
  http_close(c);
  return ERR_ABRT;
}

static err_t on_accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
  (void)arg;
  if (err != ERR_OK || pcb == NULL) return ERR_VAL;

  http_conn_t *c = (http_conn_t *)mem_calloc(1, sizeof(http_conn_t));
  if (!c) return ERR_MEM;
  c->pcb = pcb;

  tcp_arg(pcb, c);
  tcp_recv(pcb, on_recv);
  tcp_sent(pcb, on_sent);
  tcp_err(pcb, on_err);
  tcp_poll(pcb, on_poll, 8);   /* ~4 s at 2 polls/s */
  return ERR_OK;
}

void http_server_init(void)
{
  struct tcp_pcb *pcb = tcp_new();
  if (!pcb) return;
  if (tcp_bind(pcb, IP_ANY_TYPE, 80) != ERR_OK) { tcp_abort(pcb); return; }
  pcb = tcp_listen(pcb);
  tcp_accept(pcb, on_accept);
}
