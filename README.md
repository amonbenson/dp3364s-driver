# dp3364s-driver

Proof of concept for driving an RGB LED matrix built from **DP3364S** column
driver chips, using an ESP32-S3.

These panels look like HUB75 but do not behave like it. The DP3364S holds the
frame in its own memory and needs a configuration register set plus a specific
command sequence before it shows anything, and the chip is barely documented.
This project works that sequence out and drives a 128x64 panel with a scrolling
test pattern.

## How it works

The LCD_CAM peripheral runs as a 16-bit parallel port. Its clock output drives
the panel clock, the other 13 signals are data bus bits, and GDMA plays the
frame from a descriptor chain that loops on itself. The clock is therefore
generated in hardware, and after startup the panel refreshes with no CPU
involvement.

The output stream has two phases, chained together in hardware: the chip
registers are written once, then one frame of pixel data repeats indefinitely.

## Status

Working. 128x64 pixels, 10 MHz panel clock, 152 Hz refresh, 13-bit PWM per
channel with gamma correction. Column drivers: DP3364S (PWM/SRAM). Line drivers: TC7262 (Binary decoder).

## Build

```
idf.py set-target esp32s3
idf.py build flash monitor
```

Wiring is defined at the top of `main/main.c` and must match the panel.

## Drawing

`draw_frame()` is the only place the picture is decided. It fills an (x, y) RGB
framebuffer at 0..255 per channel; everything else is protocol.
