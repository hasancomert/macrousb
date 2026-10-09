/**
 * tusb_config.h -- TinyUSB configuration for macrousb.
 *
 * Composite device: one USB network interface (RNDIS by default; CDC-ECM when
 * built with -DUSE_ECM=1) plus one HID keyboard interface.
 * NCM is intentionally not used (RNDIS is the driverless choice on Windows).
 */
#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

/* MCU is provided by the Makefile (-DCFG_TUSB_MCU=OPT_MCU_STM32F4). */
#ifndef CFG_TUSB_MCU
  #error "CFG_TUSB_MCU must be defined by the build"
#endif

#ifndef BOARD_TUD_RHPORT
  #define BOARD_TUD_RHPORT 0
#endif
#ifndef BOARD_TUD_MAX_SPEED
  #define BOARD_TUD_MAX_SPEED OPT_MODE_DEFAULT_SPEED
#endif

#define CFG_TUSB_OS            OPT_OS_NONE
#define CFG_TUSB_DEBUG         0

#define CFG_TUD_ENABLED        1
#define CFG_TUD_MAX_SPEED      BOARD_TUD_MAX_SPEED

/* VBUS sensing is configured at runtime via tud_configure() in the board file. */
#ifndef CFG_TUD_VBUS_DETECT_HW
  #define CFG_TUD_VBUS_DETECT_HW 0
#endif

#ifndef CFG_TUSB_MEM_SECTION
  #define CFG_TUSB_MEM_SECTION
#endif
#ifndef CFG_TUSB_MEM_ALIGN
  #define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))
#endif

#define CFG_TUD_ENDPOINT0_SIZE 64

//------------- Classes -------------//
/* Network: ECM/RNDIS driver (not NCM). RNDIS vs ECM chosen by the config
 * descriptor; see usb_descriptors.c. */
#ifndef USE_ECM
  #define USE_ECM 0   /* 0 = RNDIS (Windows driverless), 1 = CDC-ECM (Linux/macOS) */
#endif

#define CFG_TUD_ECM_RNDIS   1
#define CFG_TUD_NCM         0

#define CFG_TUD_HID         1
#define CFG_TUD_HID_EP_BUFSIZE 16

#define CFG_TUD_CDC         0
#define CFG_TUD_MSC         0
#define CFG_TUD_MIDI        0
#define CFG_TUD_VENDOR      0

/* Ethernet MTU (payload). 1514 = 1500 + 14-byte Ethernet header. */
#ifndef CFG_TUD_NET_MTU
  #define CFG_TUD_NET_MTU 1514
#endif

#ifdef __cplusplus
}
#endif

#endif /* TUSB_CONFIG_H_ */
