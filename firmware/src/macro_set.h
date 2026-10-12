/**
 * macro_set.h -- the collection of named macros held in RAM and flash.
 */
#ifndef MACRO_SET_H_
#define MACRO_SET_H_

#include <stdint.h>

#define MACRO_NAME_MAX    24
#define MACRO_SCRIPT_MAX  512
#define MACRO_COUNT_MAX   6

typedef struct {
  char name[MACRO_NAME_MAX];
  char script[MACRO_SCRIPT_MAX];
} macro_item_t;

typedef struct {
  uint8_t  count;        /* macros in use (0..MACRO_COUNT_MAX) */
  uint8_t  selected;     /* active index                       */
  uint8_t  host;         /* host_profile_t                     */
  uint8_t  loop;         /* bool: repeat on interval           */
  uint32_t interval_ms;  /* delay between loop runs            */
  macro_item_t items[MACRO_COUNT_MAX];
} macro_set_t;

#endif /* MACRO_SET_H_ */
