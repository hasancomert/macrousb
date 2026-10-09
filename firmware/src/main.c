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

  uint32_t last = 0;
  bool led = false;

  while (1) {
    tud_task();       /* USB device stack */
    net_service();    /* lwIP timers      */

    /* Heartbeat: slow blink when mounted, fast when not. Replaced by
     * idle/running/typing states once the typer is added. */
    uint32_t now = board_millis();
    uint32_t period = tud_mounted() ? 1000 : 250;
    if (now - last >= period) {
      last = now;
      led = !led;
      board_led_write(led);
    }
  }
}

/* USB lifecycle callbacks (optional; used for LED state) */
void tud_mount_cb(void)   {}
void tud_umount_cb(void)  {}
void tud_suspend_cb(bool remote_wakeup_en) { (void)remote_wakeup_en; }
void tud_resume_cb(void)  {}
