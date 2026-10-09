/**
 * keymap.h -- ASCII character to USB HID usage code mapping.
 *
 * The mapping assumes a US QWERTY host layout. The characters produced on the
 * host depend on the host's active keyboard layout, not the device (see README).
 */
#ifndef KEYMAP_H_
#define KEYMAP_H_

#include <stdint.h>
#include <stdbool.h>

/* Map one ASCII char to a HID modifier + usage code.
 * Returns false if the character cannot be typed (non-ASCII or no key). */
bool keymap_ascii_to_hid(char ascii, uint8_t *modifier, uint8_t *keycode);

#endif /* KEYMAP_H_ */
