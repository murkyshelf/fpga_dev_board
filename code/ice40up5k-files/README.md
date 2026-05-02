# iCE40UP5K Blinky

Minimal blinky design for the custom iCE40UP5K-SG48 FPGA dev board.

The FPGA-controlled onboard LED is `D1`:

- KiCad net: `/led`
- FPGA pin: U1 pin `39`
- iCE40 function: `RGB0`
- Drive method: `SB_RGBA_DRV` constant-current LED driver

`D1` has no external series resistor. Do not drive this net as a normal
push-pull GPIO and do not short `/led` directly to ground while powered.

Build:

```sh
make
```

Program through the existing STM32 programmer bridge:

```sh
make prog PORT=/dev/ttyACM0
```

The design uses the internal `SB_HFOSC` oscillator divided to 12 MHz, so no external clock pin is required for this test.
