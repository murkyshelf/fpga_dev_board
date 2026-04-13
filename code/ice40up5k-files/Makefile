PROJ = blinky
PIN_DEF = pins.pcf
DEVICE = up5k
PACKAGE = sg48

# Serial port for programmer
PORT ?= /dev/ttyACM0

all: $(PROJ).bin

$(PROJ).json: top.v
	yosys -p 'synth_ice40 -top top -json $@' $<

$(PROJ).asc: $(PROJ).json $(PIN_DEF)
	nextpnr-ice40 --$(DEVICE) --package $(PACKAGE) --json $< --pcf $(PIN_DEF) --asc $@

$(PROJ).bin: $(PROJ).asc
	icepack $< $@

# Flashing using the provided python script
prog: $(PROJ).bin
	python3 ../stm32-programer/program.py $(PORT) $<

clean:
	rm -f $(PROJ).json $(PROJ).asc $(PROJ).bin
