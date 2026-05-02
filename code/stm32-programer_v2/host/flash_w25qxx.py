#!/usr/bin/env python3
import argparse
import binascii
import math
import sys
import time

import serial


def read_line(port: serial.Serial, timeout: float = 30.0) -> str:
    deadline = time.monotonic() + timeout
    line = bytearray()
    while time.monotonic() < deadline:
        b = port.read(1)
        if not b:
            continue
        if b == b"\n":
            return line.decode("ascii", errors="replace").strip()
        if b != b"\r":
            line.extend(b)
    raise TimeoutError("timeout waiting for device response")


def read_exact(port: serial.Serial, length: int, timeout: float = 30.0) -> bytes:
    deadline = time.monotonic() + timeout
    data = bytearray()
    while len(data) < length and time.monotonic() < deadline:
        chunk = port.read(length - len(data))
        if chunk:
            data.extend(chunk)
    if len(data) != length:
        raise TimeoutError(f"timeout reading dump data ({len(data)}/{length} bytes)")
    return bytes(data)


def command(port: serial.Serial, text: str, timeout: float = 30.0) -> str:
    port.write((text + "\n").encode("ascii"))
    port.flush()
    response = read_line(port, timeout)
    if not response.startswith("OK"):
        raise RuntimeError(f"{text}: {response}")
    return response


def parse_int(value: str) -> int:
    return int(value, 0)


def main() -> int:
    parser = argparse.ArgumentParser(description="Write a .bin to W25Qxx over STM32 USB CDC")
    parser.add_argument("--port", required=True, help="Serial port, e.g. /dev/ttyACM0 or COM5")
    parser.add_argument("--file", help="Binary file to write")
    parser.add_argument("--read", action="store_true", help="Read flash into a file instead of writing")
    parser.add_argument("--length", "--len", dest="length", type=parse_int, help="Number of bytes to read")
    parser.add_argument("--output", "--out", dest="output", help="Output file for --read")
    parser.add_argument("--addr", default="0x0", type=parse_int, help="Flash start address")
    parser.add_argument("--baud", default=115200, type=int, help="Ignored by USB CDC but kept for compatibility")
    parser.add_argument("--chunk", default=256, type=int, help="Write chunk size")
    parser.add_argument("--verify", action="store_true", help="Ask device to read back and CRC-check")
    args = parser.parse_args()

    if args.read:
        if args.length is None or args.length <= 0:
            raise SystemExit("--read requires --length/--len greater than zero")
        if not args.output:
            raise SystemExit("--read requires --output/--out")
    else:
        if not args.file:
            raise SystemExit("write mode requires --file")

    if args.chunk < 1 or args.chunk > 1024:
        raise SystemExit("--chunk must be between 1 and 1024 bytes")

    image = b""
    if not args.read:
        with open(args.file, "rb") as f:
            image = f.read()

        if not image:
            raise SystemExit("refusing to write an empty file")

    image_crc = binascii.crc32(image) & 0xFFFFFFFF
    erase_len = math.ceil(len(image) / 4096) * 4096 if image else 0

    with serial.Serial(args.port, args.baud, timeout=0.2, write_timeout=5) as port:
        time.sleep(0.5)
        port.reset_input_buffer()
        port.reset_output_buffer()

        print(command(port, "PING", timeout=5))
        print(command(port, "READID", timeout=5))

        if args.read:
            print(f"Reading 0x{args.length:X} bytes at 0x{args.addr:X}")
            response = command(port, f"READ {args.addr:X} {args.length:X}", timeout=5)
            expected = f"OK READ len={args.length}"
            if response != expected:
                raise RuntimeError(f"unexpected read response: {response}")

            data = read_exact(port, args.length, timeout=max(30, args.length / 2048))
            trailer = read_line(port, timeout=5)
            if not trailer.startswith("OK READ crc="):
                raise RuntimeError(f"unexpected read trailer: {trailer}")

            data_crc = binascii.crc32(data) & 0xFFFFFFFF
            expected_crc = int(trailer.split("crc=", 1)[1], 16)
            if data_crc != expected_crc:
                raise RuntimeError(f"dump crc mismatch: expected {expected_crc:08X} got {data_crc:08X}")

            with open(args.output, "wb") as f:
                f.write(data)

            print(f"Wrote {len(data)} bytes to {args.output}, crc32={data_crc:08X}")
            return 0

        print(f"Erasing 0x{erase_len:X} bytes at 0x{args.addr:X}")
        print(command(port, f"ERASE {args.addr:X} {erase_len:X}", timeout=max(30, erase_len / 4096 * 2)))

        print(f"Writing {len(image)} bytes, image crc32={image_crc:08X}")
        sent = 0
        while sent < len(image):
            end = min(sent + args.chunk, len(image))
            chunk = image[sent:end]
            chunk_crc = binascii.crc32(chunk) & 0xFFFFFFFF

            response = command(port, f"WRITE {args.addr + sent:X} {len(chunk):X} {chunk_crc:08X}", timeout=5)
            if response != "OK SEND":
                raise RuntimeError(f"unexpected write response: {response}")

            port.write(chunk)
            port.flush()
            response = read_line(port, timeout=10)
            if not response.startswith("OK"):
                raise RuntimeError(response)

            sent = end
            print(f"\r{sent}/{len(image)} bytes", end="")
        print()

        if args.verify:
            print("Verifying")
            print(command(port, f"VERIFY {args.addr:X} {len(image):X} {image_crc:08X}", timeout=max(30, len(image) / 2048)))

    print("Done")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, TimeoutError, serial.SerialException) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
