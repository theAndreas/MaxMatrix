# Copyright 2026 Andreas Burnickl
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The project's own targets, which the Makefile includes; the template wrote this file once, and
# `copier update` leaves it alone. A target with a `## ` comment is listed by `make`, and one
# added to PROJECT_CHECKS is part of `make check`:
#
#   PROJECT_CHECKS += sizes
#
#   sizes: ## The README's table of flash and RAM against the build
#   	tools/sizes.sh

PROJECT_CHECKS += avr

# A build of its own, as the presets of the host leave the tests avr/ out: every example compiled
# for the Uno, with bit banging and with SPI, and run in simavr against the model of the chain.
avr: ## The examples as the Uno runs them, in simavr, against simulator/Expected/
	cmake -S . -B build/avr-tests -G Ninja -DMAXMATRIX_AVR_TESTS=ON
	cmake --build build/avr-tests --target SimulatorAvr AvrExamples
	ctest --test-dir build/avr-tests -R '^avr/' --output-on-failure

# The boards beyond the AVR (OIL 46), not part of `make check`, as their cores take 8 GB: each core
# installed, then every example compiled in every variant, failing on a warning of this
# repository, as `make examples` does for the Uno. BOARDS="esp32" builds one.
BOARDS ?= uno-r4 pico esp32
boards: ## Every example for the Uno R4, the Pico and the ESP32 (OIL 46); BOARDS="esp32" for one
	MAXMATRIX_BOARDS="$(BOARDS)" tools/setup.sh
	MAXMATRIX_BOARDS="$(BOARDS)" tools/compile_examples.sh
