/**
 * flash_store.h -- persist the macro configuration in on-chip flash.
 *
 * Uses the last 128 KiB sector (sector 7 @ 0x08060000) of the F411CE, well
 * clear of the ~67 KiB firmware image. Saving erases that sector (~1-2 s, during
 * which the MCU stalls on flash access), so it is done on demand, not on every
 * change. Loading is a plain read and is instant.
 */
#ifndef FLASH_STORE_H_
#define FLASH_STORE_H_

#include <stdint.h>
#include <stdbool.h>

#define FLASH_STORE_TEXT_MAX 128

typedef struct {
  char     text[FLASH_STORE_TEXT_MAX];
  uint32_t interval_ms;
  bool     enter;
  bool     autoopen;
} flash_store_cfg_t;

/* Load config from flash into *out. Returns false (and leaves *out untouched)
 * if nothing valid is stored. */
bool flash_store_load(flash_store_cfg_t *out);

/* Erase + write *in to flash. Returns true on success. Blocks during erase. */
bool flash_store_save(const flash_store_cfg_t *in);

#endif /* FLASH_STORE_H_ */
