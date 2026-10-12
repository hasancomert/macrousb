/**
 * flash_store.h -- persist the macro set in on-chip flash (sector 7).
 *
 * Saving erases a 128 KiB sector (~1-2 s, MCU stalls on flash access), so it is
 * done on demand. Loading is a plain read and is instant.
 */
#ifndef FLASH_STORE_H_
#define FLASH_STORE_H_

#include <stdbool.h>
#include "macro_set.h"

/* Load the macro set from flash. Returns false if nothing valid is stored. */
bool flash_store_load(macro_set_t *out);

/* Erase + write the macro set to flash. Returns true on success. */
bool flash_store_save(const macro_set_t *in);

#endif /* FLASH_STORE_H_ */
