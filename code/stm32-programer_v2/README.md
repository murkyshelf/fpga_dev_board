# Blue Pill USB to W25Qxx SPI NOR flasher

This is application code for an STM32F103C8 "Blue Pill" that receives a `.bin`
file from a PC over USB CDC and writes it to an external Winbond W25Qxx SPI NOR
flash.

It is intentionally a CDC protocol rather than USB DFU. `dfu-util` can only
write external SPI flash if the firmware implements a DFU device class and maps
DFU download blocks to W25Qxx erase/write operations. CDC is much smaller and is
easy to drive from the included Python sender.

## Hardware

Default wiring uses SPI1:

| Blue Pill | W25Qxx |
| --- | --- |
| 3V3 | VCC |
| GND | GND |
| PA5 / SPI1_SCK | CLK |
| PA6 / SPI1_MISO | DO / IO1 |
| PA7 / SPI1_MOSI | DI / IO0 |
| PA4 | CS |
| 3V3 | WP / IO2 |
| 3V3 | HOLD / IO3 |

Use a 3.3 V W25Qxx module or chip. Do not connect the flash to 5 V.
This code uses standard 24-bit W25Q commands, so it supports chips up to
W25Q128 / 16 MiB. W25Q256 and larger parts need 4-byte address commands added.

## GNU Make setup, no CubeIDE

Use the standalone libopencm3 firmware in `gnu/` if CubeIDE/CubeMX does not work
for your setup:

```bash
cd gnu
git clone https://github.com/libopencm3/libopencm3.git
make -C libopencm3 TARGETS=stm32/f1
make
st-flash write build/spi_flasher.bin 0x08000000
cd ..
```

Then send your `.bin` to the external W25Qxx:

```bash
python3 host/flash_w25qxx.py --port /dev/ttyACM0 --file firmware.bin --addr 0x000000 --verify
```

## CubeMX setup

Create an STM32F103C8Tx project and enable:

- `USB_DEVICE`: Communication Device Class / CDC
- `SPI1`: Full-Duplex Master, 8-bit, mode 0, prescaler initially 8 or 16
- `PA4`: GPIO output, high by default, label `W25Q_CS`
- Clock: 72 MHz system clock from HSE if your board has an 8 MHz crystal

Copy the files from `firmware/Core/Inc` and `firmware/Core/Src` into the matching
Cube project folders.

In CubeMX-generated `Core/Src/main.c`:

```c
#include "w25qxx.h"
#include "extflash_protocol.h"
```

After `MX_USB_DEVICE_Init();` add:

```c
W25QXX_Init(&hspi1, W25Q_CS_GPIO_Port, W25Q_CS_Pin);
ExtFlashProtocol_Init();
```

In the main `while (1)` loop add:

```c
ExtFlashProtocol_Task();
```

In CubeMX-generated `USB_DEVICE/App/usbd_cdc_if.c`:

```c
#include "extflash_protocol.h"
```

Inside `CDC_Receive_FS`, before `USBD_CDC_SetRxBuffer(...)`, add:

```c
ExtFlashProtocol_Rx(Buf, *Len);
```

The full callback should still call `USBD_CDC_ReceivePacket(&hUsbDeviceFS);`.

## Flashing a binary

Install pyserial:

```bash
python3 -m pip install pyserial
```

Send and verify a binary:

```bash
python3 host/flash_w25qxx.py --port /dev/ttyACM0 --file firmware.bin --addr 0x000000 --verify
```

Dump flash contents to a file:

```bash
python3 host/flash_w25qxx.py --port /dev/ttyACM0 --read --addr 0x000000 --len 0x1A000 --out dump.bin
```

On Windows the port will look like `COM5`.

## Protocol

The CDC protocol is line based for commands and binary only for file chunks:

- `PING`
- `READID`
- `ERASE <addr_hex> <len_hex>`
- `READ <addr_hex> <len_hex>` followed by `len` raw bytes and a final CRC line
- `WRITE <addr_hex> <len_hex> <crc32_hex>` followed by exactly `len` raw bytes
- `VERIFY <addr_hex> <len_hex> <crc32_hex>`

All successful commands return a line beginning with `OK`. Failures return a
line beginning with `ERR`.

The host script sends multiple small `WRITE` commands and waits for `OK` after
each chunk. This avoids overflowing the STM32F103 receive buffer while the
W25Qxx is busy with page program operations.
