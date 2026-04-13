PROJECT = flash_programmer

CFILES = main.c
OBJS = main.o

# TODO: Adjust this path if libopencm3 is located elsewhere.
OPENCM3_DIR = ./libopencm3

DEVICE = stm32f103c8t6
OOCD_FILE = board/stm32f103c8_blue_pill.cfg

# You must run "make" in the $(OPENCM3_DIR) directory first.
include $(OPENCM3_DIR)/mk/genlink-config.mk
include $(OPENCM3_DIR)/mk/gcc-config.mk

VPATH += $(OPENCM3_DIR)/lib

INCLUDES += $(patsubst %,-I%, . $(OPENCM3_DIR)/include)

LDFLAGS += -L$(OPENCM3_DIR)/lib

all: $(PROJECT).bin

# st-flash target
flash: $(PROJECT).bin
	st-flash write $(PROJECT).bin 0x8000000

clean:
	rm -f *.o *.elf *.bin *.ld

include $(OPENCM3_DIR)/mk/genlink-rules.mk
include $(OPENCM3_DIR)/mk/gcc-rules.mk
