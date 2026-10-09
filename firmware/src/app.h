/**
 * app.h -- macro typing application: config, state machine, HTTP API, LED.
 */
#ifndef APP_H_
#define APP_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise application state (does NOT start typing). */
void app_init(void);

/* Drive the typing state machine and status LED. Call every main-loop pass. */
void app_task(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_H_ */
