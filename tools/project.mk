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
