/**
 * flash_store.c -- macro-set persistence in STM32F411 flash sector 7.
 *
 * Layout at 0x08060000: { magic, macro_set_t, crc }, programmed word-by-word.
 */
#include "flash_store.h"
#include "stm32f4xx_hal.h"
#include <string.h>

#define STORE_ADDR    0x08060000UL    /* sector 7 base (last 128 KiB) */
#define STORE_SECTOR  FLASH_SECTOR_7
#define STORE_MAGIC   0x4D554332UL     /* "MUC2" (v2 = macro set) */

typedef struct {
  uint32_t    magic;
  macro_set_t set;
  uint32_t    crc;
} persist_t;

/* persist_t must be a multiple of 4 bytes (programmed as words). */
_Static_assert((sizeof(persist_t) % 4) == 0, "persist_t must be word-aligned sized");
#define PERSIST_WORDS (sizeof(persist_t) / 4u)

static uint32_t calc_crc(const persist_t *p)
{
  const uint32_t *w = (const uint32_t *)p;
  uint32_t sum = 0;
  for (unsigned i = 0; i < PERSIST_WORDS - 1; i++) sum += w[i];
  return sum ^ 0xA5A5A5A5u;
}

bool flash_store_load(macro_set_t *out)
{
  const persist_t *p = (const persist_t *)STORE_ADDR;
  if (p->magic != STORE_MAGIC) return false;
  if (p->crc != calc_crc(p))   return false;
  if (p->set.count > MACRO_COUNT_MAX) return false;
  memcpy(out, &p->set, sizeof(*out));
  return true;
}

bool flash_store_save(const macro_set_t *in)
{
  static persist_t pt;              /* static: avoid a large stack frame */
  memset(&pt, 0, sizeof(pt));
  pt.magic = STORE_MAGIC;
  memcpy(&pt.set, in, sizeof(pt.set));
  pt.crc = calc_crc(&pt);

  HAL_FLASH_Unlock();

  FLASH_EraseInitTypeDef er = {
    .TypeErase    = FLASH_TYPEERASE_SECTORS,
    .Sector       = STORE_SECTOR,
    .NbSectors    = 1,
    .VoltageRange = FLASH_VOLTAGE_RANGE_3,
  };
  uint32_t sector_err = 0;
  bool ok = (HAL_FLASHEx_Erase(&er, &sector_err) == HAL_OK);

  if (ok) {
    const uint32_t *w = (const uint32_t *)&pt;
    for (unsigned i = 0; i < PERSIST_WORDS && ok; i++) {
      ok = (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, STORE_ADDR + 4u * i, w[i]) == HAL_OK);
    }
  }

  HAL_FLASH_Lock();
  return ok;
}
