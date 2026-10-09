# macrousb firmware -- STM32F411CEU6 "Black Pill"
# Toolchain: arm-none-eabi-gcc.  Run firmware/third_party/fetch_deps.sh first.
#
#   make            build build/macrousb.elf (+ .bin/.hex)
#   make USE_ECM=1  build with CDC-ECM instead of RNDIS (Linux/macOS)
#   make flash-st   flash via ST-Link (st-flash)
#   make flash-dfu  flash via USB DFU (dfu-util)
#   make clean

TARGET   = macrousb
BUILD    = build
TP       = third_party

CROSS    = arm-none-eabi-
CC       = $(CROSS)gcc
OBJCOPY  = $(CROSS)objcopy
SIZE     = $(CROSS)size

# ---- dependency roots ----
HAL        = $(TP)/stm32f4xx_hal_driver
CMSIS_DEV  = $(TP)/cmsis_device_f4
CMSIS_CORE = $(TP)/cmsis_core
LWIP       = $(TP)/lwip
TUSB       = $(TP)/tinyusb

# ---- options ----
USE_ECM ?= 0

# ---- MCU ----
MCUFLAGS = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

DEFS = -DSTM32F411xE -DUSE_HAL_DRIVER -DHSE_VALUE=25000000 \
       -DCFG_TUSB_MCU=OPT_MCU_STM32F4 -DUSE_ECM=$(USE_ECM)

GEN = $(BUILD)/generated/web_content.h

INC = -Isrc -Iboard \
      -I$(BUILD)/generated \
      -I$(CMSIS_CORE)/Include \
      -I$(CMSIS_DEV)/Include \
      -I$(HAL)/Inc \
      -I$(LWIP)/src/include \
      -I$(TUSB)/src \
      -I$(TUSB)/lib/networking

CFLAGS  = $(MCUFLAGS) $(DEFS) $(INC) -Og -g3 \
          -ffunction-sections -fdata-sections -fno-common \
          -Wall -std=gnu11 -MMD -MP
ASFLAGS = $(MCUFLAGS) -x assembler-with-cpp -MMD -MP

LDSCRIPT = linker/STM32F411CEUx_FLASH.ld
LDFLAGS  = $(MCUFLAGS) -specs=nano.specs -T$(LDSCRIPT) \
           -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map -Wl,--print-memory-usage

# ---- sources ----
APP_C = \
  src/main.c \
  src/usb_descriptors.c \
  src/net_glue.c \
  src/http_server.c \
  src/syscalls.c \
  board/board_f411_blackpill.c

HAL_C = \
  $(HAL)/Src/stm32f4xx_hal.c \
  $(HAL)/Src/stm32f4xx_hal_rcc.c \
  $(HAL)/Src/stm32f4xx_hal_rcc_ex.c \
  $(HAL)/Src/stm32f4xx_hal_gpio.c \
  $(HAL)/Src/stm32f4xx_hal_pwr.c \
  $(HAL)/Src/stm32f4xx_hal_pwr_ex.c \
  $(HAL)/Src/stm32f4xx_hal_cortex.c \
  $(HAL)/Src/stm32f4xx_hal_flash.c \
  $(HAL)/Src/stm32f4xx_hal_flash_ex.c

CMSIS_C = $(CMSIS_DEV)/Source/Templates/system_stm32f4xx.c
STARTUP = $(CMSIS_DEV)/Source/Templates/gcc/startup_stm32f411xe.s

LWIP_C = \
  $(wildcard $(LWIP)/src/core/*.c) \
  $(wildcard $(LWIP)/src/core/ipv4/*.c) \
  $(LWIP)/src/netif/ethernet.c

TUSB_C = \
  $(TUSB)/src/tusb.c \
  $(TUSB)/src/common/tusb_fifo.c \
  $(TUSB)/src/device/usbd.c \
  $(TUSB)/src/class/net/ecm_rndis_device.c \
  $(TUSB)/src/class/hid/hid_device.c \
  $(TUSB)/src/portable/synopsys/dwc2/dcd_dwc2.c \
  $(TUSB)/src/portable/synopsys/dwc2/dwc2_common.c \
  $(TUSB)/lib/networking/rndis_reports.c \
  $(TUSB)/lib/networking/dhserver.c \
  $(TUSB)/lib/networking/dnserver.c

C_SRCS = $(APP_C) $(HAL_C) $(CMSIS_C) $(LWIP_C) $(TUSB_C)

# Objects live flat in $(BUILD) keyed by basename (all basenames are unique).
vpath %.c $(sort $(dir $(C_SRCS)))
vpath %.s $(dir $(STARTUP))

OBJS  = $(addprefix $(BUILD)/,$(notdir $(C_SRCS:.c=.o)))
OBJS += $(BUILD)/$(notdir $(STARTUP:.s=.o))

# ---- rules ----
all: $(BUILD)/$(TARGET).elf

$(GEN): ../web/index.html ../web/tools/embed.py | $(BUILD)/generated
	python3 ../web/tools/embed.py ../web/index.html $(GEN)

$(BUILD)/%.o: %.c | $(BUILD) $(GEN)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s | $(BUILD)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	$(OBJCOPY) -O binary $@ $(BUILD)/$(TARGET).bin
	$(OBJCOPY) -O ihex   $@ $(BUILD)/$(TARGET).hex
	$(SIZE) $@

$(BUILD):
	mkdir -p $(BUILD)
$(BUILD)/generated:
	mkdir -p $(BUILD)/generated

flash-st: $(BUILD)/$(TARGET).elf
	st-flash --reset write $(BUILD)/$(TARGET).bin 0x08000000
flash-dfu: $(BUILD)/$(TARGET).elf
	dfu-util -a 0 -s 0x08000000:leave -D $(BUILD)/$(TARGET).bin

clean:
	rm -rf $(BUILD)

.PHONY: all clean flash-st flash-dfu
-include $(wildcard $(BUILD)/*.d)
