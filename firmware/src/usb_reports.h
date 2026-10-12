/**
 * usb_reports.h -- HID report IDs.
 *
 * Keyboard and mouse share ONE HID interface/endpoint (the F411 OTG-FS has only
 * 4 endpoints, all otherwise used), distinguished by report ID.
 */
#ifndef USB_REPORTS_H_
#define USB_REPORTS_H_

enum {
  REPORT_ID_KEYBOARD = 1,
  REPORT_ID_MOUSE    = 2,
};

#endif /* USB_REPORTS_H_ */
