PROJ := blinky
TOP := top
PIN_DEF := pins.pcf
DEVICE := up5k
PACKAGE := sg48

# Serial port for the STM32 programmer bridge.
PORT ?= /dev/ttyACM0

all: $(PROJ).bin

$(PROJ).json: top.v
	yosys -p 'synth_ice40 -top $(TOP) -json $@' $<

$(PROJ).asc: $(PROJ).json $(PIN_DEF)
	nextpnr-ice40 --$(DEVICE) --package $(PACKAGE) --json $< --pcf $(PIN_DEF) --asc $@

$(PROJ).bin: $(PROJ).asc
	icepack $< $@

# Flash using the existing STM32 programmer script.
prog: $(PROJ).bin
	python3 ../stm32-programer/program.py $(PORT) $<

flash: prog

clean:
	rm -f $(PROJ).json $(PROJ).asc $(PROJ).bin abc.history

.PHONY: all prog flash clean
