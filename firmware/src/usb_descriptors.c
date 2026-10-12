/**
 * usb_descriptors.c -- composite device descriptors for macrousb.
 *
 *   Interface 0/1 : USB network  (RNDIS by default, CDC-ECM with -DUSE_ECM=1)
 *   Interface 2   : HID keyboard
 *
 * Single configuration. RNDIS is enumerated so Windows binds its built-in
 * "USB RNDIS" driver without an .inf; CDC-ECM (build switch) suits Linux/macOS.
 */
#include "tusb.h"
#include "class/net/net_device.h"
#include "board.h"
#include "usb_reports.h"
#include <string.h>

/* VID/PID: 0xCafe is TinyUSB's test VID. These are NOT registered USB IDs;
 * fine for a personal device, do not ship a product with them. */
#define USB_VID   0xCafe
#define USB_PID   0x4020   /* net + HID composite */
#define USB_BCD   0x0200

/* MAC address of the device side; defined in net_glue.c. */
extern uint8_t tud_network_mac_address[6];

//--------------------------------------------------------------------+
// String indices
//--------------------------------------------------------------------+
enum {
  STRID_LANGID = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL,
  STRID_NET_ITF,
  STRID_MAC,
  STRID_HID_ITF,
  STRID_COUNT
};

//--------------------------------------------------------------------+
// Interface / endpoint numbering
//--------------------------------------------------------------------+
enum {
  ITF_NUM_NET = 0,     /* CDC control (RNDIS/ECM) */
  ITF_NUM_NET_DATA,    /* CDC data               */
  ITF_NUM_HID,         /* HID keyboard + mouse   */
  ITF_NUM_TOTAL
};

#define EPNUM_NET_NOTIF   0x81
#define EPNUM_NET_OUT     0x02
#define EPNUM_NET_IN      0x82
#define EPNUM_HID_IN      0x83

//--------------------------------------------------------------------+
// Device descriptor
//--------------------------------------------------------------------+
static tusb_desc_device_t const desc_device = {
  .bLength            = sizeof(tusb_desc_device_t),
  .bDescriptorType    = TUSB_DESC_DEVICE,
  .bcdUSB             = USB_BCD,
  /* IAD is needed to group the two CDC (network) interfaces. */
  .bDeviceClass       = TUSB_CLASS_MISC,
  .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
  .bDeviceProtocol    = MISC_PROTOCOL_IAD,
  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
  .idVendor           = USB_VID,
  .idProduct          = USB_PID,
  .bcdDevice          = 0x0100,
  .iManufacturer      = STRID_MANUFACTURER,
  .iProduct           = STRID_PRODUCT,
  .iSerialNumber      = STRID_SERIAL,
  .bNumConfigurations = 1
};

uint8_t const *tud_descriptor_device_cb(void)
{
  return (uint8_t const *)&desc_device;
}

//--------------------------------------------------------------------+
// HID report descriptor (keyboard + mouse, each with its own report ID)
//--------------------------------------------------------------------+
static uint8_t const desc_hid_report[] = {
  TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD)),
  TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(REPORT_ID_MOUSE)),
};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
  (void)instance;
  return desc_hid_report;
}

//--------------------------------------------------------------------+
// Configuration descriptor
//--------------------------------------------------------------------+
#if USE_ECM
  #define NET_DESC_LEN  TUD_CDC_ECM_DESC_LEN
#else
  #define NET_DESC_LEN  TUD_RNDIS_DESC_LEN
#endif

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + NET_DESC_LEN + TUD_HID_DESC_LEN)

static uint8_t const desc_configuration[] = {
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0, 100),

#if USE_ECM
  TUD_CDC_ECM_DESCRIPTOR(ITF_NUM_NET, STRID_NET_ITF, STRID_MAC, EPNUM_NET_NOTIF, 64,
                         EPNUM_NET_OUT, EPNUM_NET_IN, 64, CFG_TUD_NET_MTU),
#else
  TUD_RNDIS_DESCRIPTOR(ITF_NUM_NET, STRID_NET_ITF, EPNUM_NET_NOTIF, 8,
                       EPNUM_NET_OUT, EPNUM_NET_IN, 64),
#endif

  /* protocol NONE: report IDs are used, so this is not a boot keyboard */
  TUD_HID_DESCRIPTOR(ITF_NUM_HID, STRID_HID_ITF, HID_ITF_PROTOCOL_NONE,
                     sizeof(desc_hid_report), EPNUM_HID_IN, CFG_TUD_HID_EP_BUFSIZE, 10),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
  (void)index;
  return desc_configuration;
}

//--------------------------------------------------------------------+
// String descriptors
//--------------------------------------------------------------------+
static char const *const string_desc_arr[STRID_COUNT] = {
  [STRID_LANGID]       = (const char[]){0x09, 0x04},  /* English (0x0409) */
  [STRID_MANUFACTURER] = "macrousb",
  [STRID_PRODUCT]      = "MacroUSB Keyboard+Net",
  [STRID_SERIAL]       = NULL,   /* generated from STM32 UID */
  [STRID_NET_ITF]      = "MacroUSB Network",
  [STRID_MAC]          = NULL,   /* generated from MAC        */
  [STRID_HID_ITF]      = "MacroUSB Keyboard",
};

static uint16_t _desc_str[32 + 1];

static uint8_t serial_to_utf16(uint16_t *dst, uint8_t max_chars)
{
  uint8_t uid[12];
  size_t n = board_get_unique_id(uid, sizeof(uid));
  uint8_t count = 0;
  for (size_t i = 0; i < n && count + 2 <= max_chars; i++) {
    dst[count++] = "0123456789ABCDEF"[(uid[i] >> 4) & 0xF];
    dst[count++] = "0123456789ABCDEF"[(uid[i] >> 0) & 0xF];
  }
  return count;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void)langid;
  unsigned chr_count = 0;
  const uint8_t max_chars = (sizeof(_desc_str) / sizeof(_desc_str[0])) - 1;

  switch (index) {
    case STRID_LANGID:
      memcpy(&_desc_str[1], string_desc_arr[STRID_LANGID], 2);
      chr_count = 1;
      break;

    case STRID_SERIAL:
      chr_count = serial_to_utf16(&_desc_str[1], max_chars);
      break;

    case STRID_MAC:
      for (unsigned i = 0; i < 6 && chr_count + 2 <= max_chars; i++) {
        _desc_str[1 + chr_count++] = "0123456789ABCDEF"[(tud_network_mac_address[i] >> 4) & 0xF];
        _desc_str[1 + chr_count++] = "0123456789ABCDEF"[(tud_network_mac_address[i] >> 0) & 0xF];
      }
      break;

    default:
      if (index >= STRID_COUNT || string_desc_arr[index] == NULL) {
        return NULL;
      }
      {
        const char *str = string_desc_arr[index];
        chr_count = strlen(str);
        if (chr_count > max_chars) chr_count = max_chars;
        for (unsigned i = 0; i < chr_count; i++) {
          _desc_str[1 + i] = str[i];
        }
      }
      break;
  }

  _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
  return _desc_str;
}

//--------------------------------------------------------------------+
// HID class callbacks (typing logic is added in a later step)
//--------------------------------------------------------------------+
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen)
{
  (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)reqlen;
  return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize)
{
  /* Host LED state (Caps/Num lock) arrives here; ignored. */
  (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)bufsize;
}
