#!/bin/bash
#
# Copyright 2026 Andreas Burnickl
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Shared by the scripts in tools/: where things lie, and how an example is compiled in each
# variant. The defaults below are what a project gets that says nothing else; tools/project.sh,
# sourced after them, is where it says otherwise. This file comes from the template and is
# brought up to date with `copier update`; what is the project's goes into tools/project.sh.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly ROOT_DIR
readonly BUILD_DIR="${ROOT_DIR}/build"

# The defaults, each described in tools/project.sh.
# shellcheck disable=SC2034  # used by the scripts that source this file
{
  NAMESPACE=""
  FQBN="arduino:avr:uno"
  ARDUINO_CORE="arduino:avr"
  ARDUINO_BOARD_URLS=""
  SIMAVR_MCU="atmega328p"
  SIMAVR_FREQUENCY=16000000
  DATABASE_EXAMPLE="Basics"
  SELF_CHECK_EXAMPLE="SelfCheck"
  VARIANTS=(default)
  TIDY_VARIANTS=(default)
  DOCS_INTERNAL_API=""
  CPPCHECK=1
}

# The hooks, which do nothing unless tools/project.sh defines them again, and
# then are never called: SC2329.
# shellcheck disable=SC2329
{
  variant_flags() {
    :
  }

  prepare() {
    :
  }

  check_elf() {
    :
  }
}

# shellcheck source=tools/project.sh
source "${ROOT_DIR}/tools/project.sh"

err() {
  echo "$*" >&2
}

# Prints the name of every example, one per line.
list_examples() {
  local dir
  for dir in "${ROOT_DIR}"/examples/*/; do
    basename "${dir}"
  done
}

# Prints the bin directory of the avr-gcc that arduino-cli uses for FQBN.
avr_gcc_bin_dir() {
  arduino-cli compile --fqbn "${FQBN}" --show-properties \
      "${ROOT_DIR}/examples/${DATABASE_EXAMPLE}" 2>/dev/null \
    | sed -n 's/^compiler\.path=//p'
}

# Compiles an example in a variant into BUILD_DIR/<variant>/<example> and prints what
# arduino-cli prints. The flags of the variant go into compiler.cpp.extra_flags, which reaches
# every C++ file of the build, the core's as well.
# Arguments:
#   variant: one of VARIANTS
#   example: the name of a folder in examples/
compile_example() {
  local variant="$1"
  local example="$2"
  local args=(
    --fqbn "${FQBN}"
    --library "${ROOT_DIR}"
    --warnings all
    --build-path "${BUILD_DIR}/${variant}/${example}"
  )
  local flags
  flags="$(variant_flags "${variant}" | paste -sd ' ' -)"
  if [[ -n "${flags}" ]]; then
    args+=(--build-property "compiler.cpp.extra_flags=${flags}")
  fi
  arduino-cli compile "${args[@]}" "${ROOT_DIR}/examples/${example}" 2>&1
}

# Prints the path of the ELF file that compile_example made.
elf_of() {
  local variant="$1"
  local example="$2"
  echo "${BUILD_DIR}/${variant}/${example}/${example}.ino.elf"
}
