# Checking on hardware

The tests reach the LEDs only through the model in `test/mock/` and the real binary only in simavr. Neither has a
MAX7219's timing on a real wire, nor a module that is wired other than its datasheet says. So a change to the
transfer code is untested until it has run on a matrix, and so is a release. This is what to run then, and what each
step has to show.

The board is an FC-16 of four modules on an Uno, with DIN at pin 11, CS at 10 and CLK at 13. Pins 11 and 13 are the
Uno's SPI pins as well, so the same wiring serves bit banging and SPI. Write down which steps passed, on which board,
in the pull request or the release.

## Flashing both transfer paths

From the Arduino IDE, with this repository as `MaxMatrix` in the sketchbook's `libraries` folder. The IDE passes a
sketch's defines to no library, so for SPI set `MAXMATRIX_USE_SPI` to `STD_ON` in `src/MaxMatrixConfig.h` for the
time of the check, and back afterwards. With `arduino-cli` no file changes:

```sh
arduino-cli compile --fqbn arduino:avr:uno --library . --upload --port COM3 examples/ScrollText
arduino-cli compile --fqbn arduino:avr:uno --library . --upload --port COM3 \
    --build-property "compiler.cpp.extra_flags=-DMAXMATRIX_USE_SPI=STD_ON" examples/ScrollText
```

The dev container sees no USB port under WSL2 unless `usbipd` attaches it and the container is given the device.

## 1. The examples, with bit banging and with SPI

Each of them once with each transfer path.

- [ ] **Basics**: "Hi!" upright and not mirrored; a diagonal that grows dot by dot across all four modules; row 3
      on every module; a frame that appears at once; the intensity from 0 to 15.
- [ ] **ScrollText**: "Hello World!" to the left without end, and no halt and no gap but its three spaces where it
      starts over.
- [ ] **Shapes**: one a second, a frame with a filled box, an X across the panel, circles of radius 0 to 3, four
      hearts.
- [ ] **Messages**: the texts in turn, each at its own pace, the umlauts as they are written.
- [ ] **Animation**: a ball off the edges, without flicker.
- [ ] **Shutdown**: what the comments in the sketch say, step by step: dark while shut down, the text drawn in the
      dark when the LEDs come on again, every LED at full brightness in the display test, then the text at its
      intensity.

Optionally, with the board upside down and `SetMatrixOrientation(MaxMatrixMatrixOrientation::k180)` added to
`setup()` of Basics:

- [ ] the text stands upright again.

## 2. SerialControl and the control program

- [ ] In a serial monitor at 115200 baud with "Newline": `ready` after a reset; `text=Hallo` answers `ok` and shows
      the text; `intensity=16` answers `error: ` and a reason; `picture` answers `picture=` and 64 hex digits.
- [ ] The control program on Linux, with the board: connect its port, send a text, *Shift in*, the sliders, click
      LEDs and paint with the button held. The panel in the window shows what the board shows.
- [ ] The control program on Windows: built with `cmake -S control -B build-control` against wxWidgets 3.2 for
      MinGW, then the same as on Linux.

## Where a failure points

| What the matrix does | Where to look |
|---|---|
| Dark or garbage with bit banging, right with SPI | The port registers or the pulses being too short for the module, `MaxMatrixChain::ShiftOut()` |
| Dark with both | The wiring, chip select, `MaxMatrixChain::Init()` |
| Only the first module right, the others shifted | The frame length, `MaxMatrixChain::WriteRegisterEach()` and its stride |
| Text turned or mirrored | The module type, `MaxMatrixPanel::ConvertDotToBuffer()` |
| SerialControl loses chars or does not answer | The interrupts held off too long while the pins are written |
