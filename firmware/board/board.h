/**
 * board.h -- board abstraction for macrousb.
 *
 * ALL board-dependent definitions live here and in the matching board_*.c.
 * To port to another STM32F4/F7, provide a new board_<name>.c implementing
 * this API and adjust the constants below (and the Makefile's CFG_TUSB_MCU,
 * startup file and linker script).
 *
 * Target: STM32F411CEU6 "Black Pill" V2.0  (WeAct)
 *   - 25 MHz HSE -> 84 MHz SYSCLK, 48 MHz USB
 *   - User LED    : PC13 (active-low)
 *   - User button : PA0  (active-low, "KEY")
 *   - USB OTG FS  : PA11 (D-) / PA12 (D+), AF10, VBUS sensing disabled
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
