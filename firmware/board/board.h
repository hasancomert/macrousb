/**
 * board.h -- board abstraction for macrousb.
 *
 * ALL board-dependent definitions live here and in board_blackpill.c.
 * The Makefile's MCU profile (f411 / f401rc) picks the device define, startup,
 * linker script and HSE_VALUE; the pin map below is common to these Black Pills.
 *
 * Target: STM32F4 "Black Pill" (WeAct-style)
 *   - STM32F411CEU6 (512 KB / 128 KB)  -- make MCU=f411   (default)
 *   - STM32F401RCT6 (256 KB /  64 KB)  -- make MCU=f401rc
 *   - HSE crystal   : PLLM auto-derives from HSE_VALUE (25 MHz default; pass
 *                     HSE_VALUE=8000000 if the board has an 8 MHz crystal)
 *   - User LED      : PC13 (active-low)
 *   - User button   : PA0  (active-low, "KEY")
 *   - USB OTG FS    : PA11 (D-) / PA12 (D+), AF10, VBUS sensing disabled
 */
#ifndef BOARD_H_
#define BOARD_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TinyUSB root-hub port for the device stack (OTG_FS = port 0). */
#define BOARD_TUD_RHPORT  0

/* Bring the MCU up: clocks, flash cache, GPIO, USB pins, 1 ms SysTick.
 * Must be called before tusb_init(). */
void board_init(void);

/* LED: true = visibly ON (polarity handled internally). */
void board_led_write(bool on);

/* User button: true while pressed. */
bool board_button_read(void);

/* Milliseconds since board_init() (1 ms SysTick). */
uint32_t board_millis(void);

/* 96-bit STM32 unique id -> up to max_len bytes; returns bytes written. */
size_t board_get_unique_id(uint8_t id[], size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H_ */
