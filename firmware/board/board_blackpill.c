/**
 * board_blackpill.c -- STM32F4 "Black Pill" board support (F411CEU6 / F401RCT6).
 *
 * Pins (PC13 LED, PA0 button, PA11/PA12 USB) are common to these boards; the
 * PLL derives from HSE_VALUE so the same clock code fits 25 MHz or 8 MHz
 * crystals. The MCU density (flash/RAM), startup and linker come from the
 * Makefile's MCU profile. Bring-up adapted from TinyUSB's hw/bsp/stm32f4 (MIT).
 */
#include "board.h"
#include "stm32f4xx_hal.h"
#include "tusb.h"

/* -------- board-dependent pin map -------- */
#define LED_PORT        GPIOC
#define LED_PIN         GPIO_PIN_13
#define LED_ON_STATE    GPIO_PIN_RESET   /* PC13 LED is active-low */

#define BUTTON_PORT     GPIOA
#define BUTTON_PIN      GPIO_PIN_0
#define BUTTON_PRESSED  GPIO_PIN_RESET   /* PA0 "KEY" is active-low */

/* -------- 1 ms time base -------- */
static volatile uint32_t s_ticks = 0;

/* HSE -> PLLM (=HSE_MHz) gives 1 MHz VCO input -> VCO 336 MHz ->
 * SYSCLK 84 MHz (/4), USB 48 MHz (/7). Works for 25 MHz or 8 MHz crystals. */
static void board_clock_init(void)
{
  RCC_OscInitTypeDef osc = {0};
  RCC_ClkInitTypeDef clk = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  osc.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
  osc.HSEState            = RCC_HSE_ON;
  osc.PLL.PLLState        = RCC_PLL_ON;
  osc.PLL.PLLSource       = RCC_PLLSOURCE_HSE;
  osc.PLL.PLLM            = HSE_VALUE / 1000000U;  /* 25 */
  osc.PLL.PLLN            = 336;
  osc.PLL.PLLP            = RCC_PLLP_DIV4;         /* 84 MHz SYSCLK */
  osc.PLL.PLLQ            = 7;                     /* 48 MHz USB    */
  HAL_RCC_OscConfig(&osc);

  clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                       RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
  clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  clk.APB1CLKDivider = RCC_HCLK_DIV2;
  clk.APB2CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2);
}

void board_init(void)
{
  board_clock_init();

  __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
  __HAL_FLASH_DATA_CACHE_ENABLE();
  __HAL_FLASH_PREFETCH_BUFFER_ENABLE();

  SystemCoreClockUpdate();

  /* GPIO clocks */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();

  GPIO_InitTypeDef g = {0};

  /* LED */
  g.Pin   = LED_PIN;
  g.Mode  = GPIO_MODE_OUTPUT_PP;
  g.Pull  = GPIO_PULLUP;
  g.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(LED_PORT, &g);
  board_led_write(false);

  /* Button */
  g.Pin  = BUTTON_PIN;
  g.Mode = GPIO_MODE_INPUT;
  g.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BUTTON_PORT, &g);

  /* 1 ms SysTick (SystemCoreClock now 84 MHz) */
  SysTick_Config(SystemCoreClock / 1000U);

  /* ---- USB OTG FS pins ---- */
  /* D-/D+ on PA11/PA12, AF10 */
  g.Pin       = GPIO_PIN_11 | GPIO_PIN_12;
  g.Mode      = GPIO_MODE_AF_PP;
  g.Pull      = GPIO_NOPULL;
  g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
  g.Alternate = GPIO_AF10_OTG_FS;
  HAL_GPIO_Init(GPIOA, &g);

  __HAL_RCC_USB_OTG_FS_CLK_ENABLE();

  /* Tell TinyUSB's dwc2 driver that VBUS sensing is NOT wired on this board. */
  tud_configure_dwc2_t cfg = CFG_TUD_CONFIGURE_DWC2_DEFAULT;
  cfg.vbus_sensing = false;
  tud_configure(BOARD_TUD_RHPORT, TUD_CFGID_DWC2, &cfg);
}

void board_led_write(bool on)
{
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, on ? LED_ON_STATE : !LED_ON_STATE);
}

bool board_button_read(void)
{
  return HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN) == BUTTON_PRESSED;
}

uint32_t board_millis(void)
{
  return s_ticks;
}

size_t board_get_unique_id(uint8_t id[], size_t max_len)
{
  const volatile uint32_t *uid = (const volatile uint32_t *)UID_BASE;
  size_t n = (max_len < 12) ? max_len : 12;
  for (size_t i = 0; i < n; i++) {
    id[i] = (uint8_t)(uid[i / 4] >> (8 * (i % 4)));
  }
  return n;
}

/* -------- interrupt handlers (override weak startup defaults) -------- */
void SysTick_Handler(void)
{
  HAL_IncTick();   /* keep HAL_GetTick() advancing for HAL timeouts */
  s_ticks++;
}

void OTG_FS_IRQHandler(void)
{
  tusb_int_handler(BOARD_TUD_RHPORT, true);
}

/* TinyUSB (osal_none) time source. */
uint32_t tusb_time_millis_api(void)
{
  return s_ticks;
}
