PROJECT := spi_flasher
OPENCM3_DIR ?= libopencm3
PREFIX ?= arm-none-eabi

CC := $(PREFIX)-gcc
OBJCOPY := $(PREFIX)-objcopy
SIZE := $(PREFIX)-size

BUILD_DIR := build
SRC_DIR := src

SRCS := \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/usb_cdc.c \
	$(SRC_DIR)/protocol.c \
	$(SRC_DIR)/w25qxx.c \
	$(SRC_DIR)/syscalls.c

OBJS := $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

CFLAGS := -Os -g3 -std=c11 -Wall -Wextra
CFLAGS += -mcpu=cortex-m3 -mthumb -msoft-float
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -DSTM32F1
CFLAGS += -I$(OPENCM3_DIR)/include -I$(SRC_DIR)
CFLAGS += -MMD -MP

LDFLAGS := -mcpu=cortex-m3 -mthumb -msoft-float
LDFLAGS += -L$(OPENCM3_DIR)/lib -Tstm32f103c8.ld -nostartfiles
LDFLAGS += -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map

LDLIBS := -lopencm3_stm32f1 -lc -lgcc

.PHONY: all clean libopencm3 flash

all: $(BUILD_DIR)/$(PROJECT).elf $(BUILD_DIR)/$(PROJECT).bin

$(BUILD_DIR)/$(PROJECT).elf: $(OBJS) stm32f103c8.ld
	$(CC) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@
	$(SIZE) $@

$(BUILD_DIR)/$(PROJECT).bin: $(BUILD_DIR)/$(PROJECT).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

libopencm3:
	$(MAKE) -C $(OPENCM3_DIR) TARGETS=stm32/f1

flash: $(BUILD_DIR)/$(PROJECT).bin
	st-flash write $< 0x08000000

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
