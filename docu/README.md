# How the modules are wired

A sketch never sees this: it works in the coordinates of [Coordinates.svg](Coordinates.svg), which are the same
for every module once the module type fits it. The pictures below are for working on `src/Matrix/`, where
`max_matrix::internal::Panel::ConvertDotToBuffer()` maps a dot as the viewer sees it to a byte and a bit of `matrix_buffer_`.

Each LED is labelled `byte.bit`. Byte *n* of a module is digit register *n* of its MAX7219 and
`matrix_buffer_[8 * module + n]`; the library sends the same byte of every module in one frame. A blue band joins
the LEDs of one byte, and the darker LED is its bit 0. All pictures are in matrix orientation 0; matrix
orientation 180 turns the whole panel on top of them. There is one per module type, named as in MD_MAX72XX.

## DR0CR0RR0

![Module type DR0CR0RR0](Wiring_DR0CR0RR0.svg)

## DR0CR0RR1 - the buffer as the viewer sees it

![Module type DR0CR0RR1](Wiring_DR0CR0RR1.svg)

## DR0CR1RR0 - Generic

![Module type DR0CR1RR0](Wiring_DR0CR1RR0.svg)

## DR0CR1RR1

![Module type DR0CR1RR1](Wiring_DR0CR1RR1.svg)

## DR1CR0RR0 - FC-16

![Module type DR1CR0RR0](Wiring_DR1CR0RR0.svg)

## DR1CR0RR1

![Module type DR1CR0RR1](Wiring_DR1CR0RR1.svg)

## DR1CR1RR0 - Parola

![Module type DR1CR1RR0](Wiring_DR1CR1RR0.svg)

## DR1CR1RR1 - ICStation

![Module type DR1CR1RR1](Wiring_DR1CR1RR1.svg)

## Where the pictures come from

`DocuPictures.cpp` draws them, with `DocuSvg` for the SVG. It places every LED by calling the library and asking the
MAX7219 model in `test/mock/` which register bit of which chip it latched; the module types in the titles are the
wirings of the model on which a pattern drawn with that module type appears upright. So no picture is drawn by hand,
and none can say what the library does not do.

After a change that moves a dot, write them again:

```sh
build/DocuPictures --write docu
```

The test `docu/pictures` runs `DocuPictures --check docu` and fails as long as a picture here is not what the library
draws.
