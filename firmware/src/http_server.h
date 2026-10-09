/**
 * http_server.h -- tiny HTTP/1.0 server on lwIP raw TCP.
 *
 * Serves the single embedded settings page and dispatches "/api/" requests to
 * app_http_api() (implemented by the application; weak default returns 404).
 */
#ifndef HTTP_SERVER_H_
#define HTTP_SERVER_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Start listening on TCP :80. Call after net_init(). */
void http_server_init(void);

/* Application hook for "/api/..." endpoints.
 *   method    : "GET" or "POST"
 *   path      : e.g. "/api/status" (no query string)
 *   query     : part after '?', or NULL
 *   body      : write a JSON response body here
 *   body_cap  : capacity of body
 * Returns number of bytes written to body, or -1 if the path is not handled
 * (server then replies 404). Weak default returns -1.
 */
int app_http_api(const char *method, const char *path, const char *query,
                 char *body, int body_cap);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_SERVER_H_ */
