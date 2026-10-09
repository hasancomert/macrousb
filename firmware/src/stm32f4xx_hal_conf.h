/**
 * Minimal HAL configuration for the macrousb firmware (STM32F411xE).
 * Only the modules actually used (RCC/GPIO/PWR/FLASH/CORTEX) are enabled to
 * keep the image small. HSE_VALUE matches the Black Pill's 25 MHz crystal.
 */
#ifndef STM32F4xx_HAL_CONF_H
#define STM32F4xx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ########## Module Selection ########## */
#define HAL_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED   /* pulled in transitively by hal.c */

/* ########## Oscillator Values (Hz) ########## */
#if !defined(HSE_VALUE)
  #define HSE_VALUE    25000000U  /* Black Pill V2.0 crystal */
#endif
#if !defined(HSE_STARTUP_TIMEOUT)
  #define HSE_STARTUP_TIMEOUT    100U
#endif
#if !defined(HSI_VALUE)
  #define HSI_VALUE    16000000U
#endif
#if !defined(LSI_VALUE)
  #define LSI_VALUE    32000U
#endif
#if !defined(LSE_VALUE)
  #define LSE_VALUE    32768U
#endif
#if !defined(LSE_STARTUP_TIMEOUT)
  #define LSE_STARTUP_TIMEOUT    5000U
#endif
#if !defined(EXTERNAL_CLOCK_VALUE)
  #define EXTERNAL_CLOCK_VALUE    12288000U
#endif

/* ########## System Configuration ########## */
#define VDD_VALUE                    3300U
#define TICK_INT_PRIORITY            0x0FU
#define USE_RTOS                     0U
#define PREFETCH_ENABLE              1U
#define INSTRUCTION_CACHE_ENABLE     1U
#define DATA_CACHE_ENABLE            1U

#define USE_HAL_RCC_REGISTER_CALLBACKS   0U

/* ########## Assert Selection ########## */
/* USE_FULL_ASSERT intentionally left undefined -> assert_param is a no-op. */

/* ########## Includes ########## */
#ifdef HAL_RCC_MODULE_ENABLED
  #include "stm32f4xx_hal_rcc.h"
#endif
#ifdef HAL_GPIO_MODULE_ENABLED
  #include "stm32f4xx_hal_gpio.h"
#endif
#ifdef HAL_DMA_MODULE_ENABLED
  #include "stm32f4xx_hal_dma.h"
#endif
#ifdef HAL_CORTEX_MODULE_ENABLED
  #include "stm32f4xx_hal_cortex.h"
#endif
#ifdef HAL_FLASH_MODULE_ENABLED
  #include "stm32f4xx_hal_flash.h"
#endif
#ifdef HAL_PWR_MODULE_ENABLED
  #include "stm32f4xx_hal_pwr.h"
#endif

#ifdef USE_FULL_ASSERT
  #define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
  void assert_failed(uint8_t *file, uint32_t line);
#else
  #define assert_param(expr) ((void)0U)
#endif

#ifdef __cplusplus
}
#endif

#endif /* STM32F4xx_HAL_CONF_H */
