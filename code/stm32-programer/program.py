import serial
import time
import argparse
import sys

def spi_cs_low(ser):
    ser.write(b'\x23')
    recv_ack(ser)

def spi_cs_high(ser):
    ser.write(b'\x24')
    recv_ack(ser)

def spi_transfer(ser, data):
    length = len(data)
    ser.write(b'\x22')
    ser.write(length.to_bytes(2, 'little'))
    ser.write(data)
    return ser.read(length)

def recv_ack(ser):
    res = ser.read(1)
    if res != b'\x06':
        print(f"Error: Did not receive ACK, got {res}")
        sys.exit(1)

def fpga_reset(ser, state):
    if state:
        ser.write(b'\x11')
    else:
        ser.write(b'\x10')
    recv_ack(ser)

def flash_write_enable(ser):
    ser.write(b'\x20')
    recv_ack(ser)

def flash_wait_busy(ser):
    while True:
        spi_cs_low(ser)
        res = spi_transfer(ser, b'\x05\x00')
        spi_cs_high(ser)
        if (res[1] & 0x01) == 0:
            break
        time.sleep(0.01)

def flash_erase_sector(ser, address):
    flash_write_enable(ser)
    spi_cs_low(ser)
    spi_transfer(ser, bytes([0x20, (address >> 16) & 0xFF, (address >> 8) & 0xFF, address & 0xFF]))
    spi_cs_high(ser)
    flash_wait_busy(ser)

def flash_page_program(ser, address, data):
    flash_write_enable(ser)
    spi_cs_low(ser)
    cmd = bytes([0x02, (address >> 16) & 0xFF, (address >> 8) & 0xFF, address & 0xFF])
    spi_transfer(ser, cmd + data)
    spi_cs_high(ser)
    flash_wait_busy(ser)

def main():
    parser = argparse.ArgumentParser(description='iCE40 SPI Programmer via STM32 BluePill')
    parser.add_argument('port', help='Serial port')
    parser.add_argument('bitstream', help='Bitstream to write (.bin)')
    args = parser.parse_args()

    with open(args.bitstream, 'rb') as f:
        data = f.read()

    try:
        ser = serial.Serial(args.port, 115200, timeout=2)
    except Exception as e:
        print(f"Failed to open port {args.port}: {e}")
        return

    # Identify
    ser.write(b'\x01')
    res = ser.read(1)
    if res != b'\x42':
        print("Programmer not found or bad response.")
        return
    print("Programmer identified.")

    # Put FPGA in reset
    fpga_reset(ser, False)

    # Wake up flash in case it's in deep power down
    spi_cs_low(ser)
    spi_transfer(ser, b'\xAB')
    spi_cs_high(ser)
    time.sleep(0.01)

    # Read JEDEC ID
    spi_cs_low(ser)
    jedec = spi_transfer(ser, b'\x9F\x00\x00\x00')
    spi_cs_high(ser)
    print(f"SPI Flash JEDEC ID: {jedec[1:].hex()}")

    # Program
    print("Erasing flash...")
    flash_wait_busy(ser)

    length = len(data)
    for addr in range(0, length, 4096):
        print(f"Erasing sector 0x{addr:06X}")
        flash_erase_sector(ser, addr)

    print("Writing pages...")
    for addr in range(0, length, 256):
        page_data = data[addr:addr+256]
        # Pad page to 256 bytes if necessary
        if len(page_data) < 256:
            page_data += b'\xFF' * (256 - len(page_data))
        if addr % 4096 == 0:
            print(f"Writing page   0x{addr:06X} / 0x{length:06X}")
        flash_page_program(ser, addr, page_data)

    print("Done. Releasing FPGA reset.")
    fpga_reset(ser, True)
    ser.close()

if __name__ == '__main__':
    main()
