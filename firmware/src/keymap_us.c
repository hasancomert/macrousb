/**
 * keymap_us.c -- US QWERTY ASCII -> HID usage code table.
 *
 * Uses TinyUSB's HID_ASCII_TO_KEYCODE table: conv[c] = { needs_shift, keycode }.
 * To support another layout, replace this file.
 */
#include "keymap.h"
#include "class/hid/hid.h"

static const uint8_t conv[128][2] = { HID_ASCII_TO_KEYCODE };

bool keymap_ascii_to_hid(char ascii, uint8_t *modifier, uint8_t *keycode)
{
  uint8_t c = (uint8_t)ascii;
  if (c >= 128) return false;

  uint8_t key = conv[c][1];
  if (key == 0) return false;            /* no key for this character */

  *modifier = conv[c][0] ? KEYBOARD_MODIFIER_LEFTSHIFT : 0;
  *keycode  = key;
  return true;
}
