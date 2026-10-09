/**
 * main.c -- macrousb entry point.
 *
 * Brings up USB (composite RNDIS/ECM network + HID keyboard), the on-device
 * DHCP/DNS servers and the HTTP settings server, then runs the cooperative
 * super-loop. Does NOT type anything on boot (typing is started from the web
 * UI; wired in a later step).
 */
#include "board.h"
#include "tusb.h"
#include "net_glue.h"
#include "http_server.h"
#include "app.h"

int main(void)
{
  board_init();

  tusb_rhport_init_t dev_init = {
    .role  = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO,
  };
  tusb_init(BOARD_TUD_RHPORT, &dev_init);

  net_init();
  http_server_init();
  app_init();

  while (1) {
    tud_task();       /* USB device stack */
    net_service();    /* lwIP timers      */
    app_task();       /* typing state machine + status LED */
  }
}

/* USB lifecycle callbacks (optional; used for LED state) */
void tud_mount_cb(void)   {}
void tud_umount_cb(void)  {}
void tud_suspend_cb(bool remote_wakeup_en) { (void)remote_wakeup_en; }
void tud_resume_cb(void)  {}
