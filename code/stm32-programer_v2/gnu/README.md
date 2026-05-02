# GNU Make firmware, no CubeIDE

This folder is a libopencm3-based firmware for STM32F103C8 Blue Pill. It builds
with `arm-none-eabi-gcc` and does not use CubeIDE, CubeMX, or STM32 HAL.

## Dependencies

Install:

- `arm-none-eabi-gcc`
- `arm-none-eabi-binutils`
- `arm-none-eabi-newlib`
- `make`
- `git`

Get libopencm3 inside this folder:

```bash
cd gnu
git clone https://github.com/libopencm3/libopencm3.git
make -C libopencm3 TARGETS=stm32/f1
```

Build the flasher firmware:

```bash
make
```

Output files:

- `build/spi_flasher.elf`
- `build/spi_flasher.bin`

Flash the Blue Pill firmware with ST-Link:

```bash
st-flash write build/spi_flasher.bin 0x08000000
```

Then send your external-flash image from the repo root:

```bash
python3 host/flash_w25qxx.py --port /dev/ttyACM0 --file firmware.bin --addr 0x000000 --verify
```

## Pins

| Blue Pill | W25Qxx |
| --- | --- |
| PA5 | CLK |
| PA6 | DO / MISO |
| PA7 | DI / MOSI |
| PA4 | CS |
| 3V3 | VCC, WP, HOLD |
| GND | GND |

This code uses W25Q 24-bit address commands and supports flash parts up to
W25Q128 / 16 MiB.

