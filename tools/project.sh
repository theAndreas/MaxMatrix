#!/bin/bash
#
# Copyright 2026 Andreas Burnickl
# SPDX-License-Identifier: GPL-3.0-or-later
#
# What the scripts in tools/ need to know of this project. tools/common.sh sources it after
# its own defaults, so what is defined here wins. The template wrote this file once; it is the
# project's, and `copier update` leaves it alone. What it may define:
#
#   NAMESPACE            the library's namespace; tools/docs.sh lets gaps in
#                        NAMESPACE::internal pass
#   FQBN                 the board the examples are compiled for
#   ARDUINO_CORE         the core, pinned, as arduino-cli installs it
#   ARDUINO_BOARD_URLS   the index of a third-party core, for arduino-cli core update-index
#   SIMAVR_MCU           what simavr runs examples/SelfCheck as; empty: no run in simavr
#   SIMAVR_FREQUENCY     its clock, in Hz
#   DATABASE_EXAMPLE     the example whose compiler flags tools/tidy.sh takes for the rest
#   SELF_CHECK_EXAMPLE   the example tools/self_check.sh runs
#   VARIANTS             the builds every example is compiled in, see variant_flags()
#   TIDY_VARIANTS        the variants clang-tidy and cppcheck read: those whose defines change
#                        the code. The first is named default: clangd reads its database
#   DOCS_INTERNAL_API    a regex of the files in NAMESPACE::internal that are API after all,
#                        such as a base a public class takes members from
#   CPPCHECK             0 if cppcheck cannot parse the library, with the reason; else 1
#
#   variant_flags VARIANT   prints the compiler flags of a variant, one per line
#   prepare                 builds what compiling needs beyond the core, such as a plugin
#   check_elf VARIANT ELF   fails if a compiled example is wrong in a way no warning says

# shellcheck disable=SC2034  # read by the scripts that source this file

NAMESPACE="max_matrix"
FQBN="arduino:avr:uno"
ARDUINO_CORE="arduino:avr@1.8.8"
ARDUINO_BOARD_URLS=""
# No examples/SelfCheck: the tests avr/ of CMakeLists.txt run every example in simavr against
# the model of the chain and compare the panel, which says more than a SelfCheck could, and
# `make avr` runs them. So tools/self_check.sh has nothing to run.
SIMAVR_MCU=""
SIMAVR_FREQUENCY="16000000"

# The boards beyond the AVR (OIL 46), which `make boards` and the workflow boards.yml compile
# every example for, each alone, as their cores take 8 GB: TARGETS stays the Uno, the default
# target, for every other check, which compiles the code that differs on them, the pins through
# digitalWrite(), as the variant portable. MAXMATRIX_BOARDS names the boards to build instead.
if [[ -n "${MAXMATRIX_BOARDS:-}" ]]; then
  read -r -a TARGETS <<< "${MAXMATRIX_BOARDS}"
fi
target_settings() {
  case "$1" in
    uno-r4)
      FQBN="arduino:renesas_uno:minima"
      ARDUINO_CORE="arduino:renesas_uno@1.6.0"
      ;;
    pico)
      FQBN="rp2040:rp2040:rpipico"
      ARDUINO_CORE="rp2040:rp2040@6.3.2"
      ARDUINO_BOARD_URLS="https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json"
      ;;
    esp32)
      FQBN="esp32:esp32:esp32"
      ARDUINO_CORE="esp32:esp32@3.3.12"
      ARDUINO_BOARD_URLS="https://espressif.github.io/arduino-esp32/package_esp32_index.json"
      ;;
  esac
}
# SerialControl reaches the most of the library: the text, the commands and Serial.
DATABASE_EXAMPLE="SerialControl"

# Bit banging by default; SPI is the other transfer path, separate code; det compiles in the
# reports of MAXMATRIX_DEV_ERROR_DETECT, the weak MaxMatrixReportError(), F() in the facade and
# its Serial, which only meet avr-gcc here. Each takes other branches of #if, so clang-tidy and
# cppcheck read all three.
# portable: bit banging through digitalWrite(), as on every board but the AVR, on the Uno.
VARIANTS=(default spi det portable)
TIDY_VARIANTS=(default spi det portable)
variant_flags() {
  case "$1" in
    spi) echo "-DMAXMATRIX_USE_SPI=STD_ON" ;;
    det) echo "-DMAXMATRIX_DEV_ERROR_DETECT=STD_ON" ;;
    portable) echo "-DMAXMATRIX_USE_PORT_REGISTERS=STD_OFF" ;;
  esac
}

# The simulator, the control program and what draws the pictures of docu/: host code beside the
# library, built by CMakeLists.txt and formatted and checked as src/ is.
HOST_CODE=(simulator control docu)
