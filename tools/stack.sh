#!/bin/bash
#
# Copyright 2026 Andreas Burnickl
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Checks that the deepest stack of every program, the examples of a library or the image of a
# firmware, fits the RAM it leaves free, on every AVR target that CMake builds, in each variant:
# tools/avr_stack.py adds it up from the call graphs avr-gcc writes beside the objects. Builds
# first, as make examples does. Leaves out a target that CMake does not build: arduino:avr's
# avr-gcc, and PlatformIO's atmelavr's, is 7.3, and -fcallgraph-info came with GCC 10; and one
# that CMake builds for another processor than an AVR. tools/stack.toml, where the project has
# one, says what a call through a pointer never reaches, as avr_stack.py describes it; when every
# program passed, tools/stack_cuts.py checks what each of its rules cut against
# tools/stack_cuts.txt, and with --fix writes it in.
#
# Usage: tools/stack.sh [--fix]

set -euo pipefail

# shellcheck source=tools/common.sh
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

main() {
  local fix=()
  if [[ "${1:-}" == --fix ]]; then
    fix=(--fix)
    shift
  fi
  if [[ $# -gt 0 ]]; then
    err "Usage: $0 [--fix]"
    exit 2
  fi

  local failed=0
  local checked=0
  local target
  local variant
  local label
  local listed
  local programs
  local program
  local mcu
  local elf
  local graphs=()
  local never=()
  local rules=()
  local -r cuts="${BUILD_DIR}/stack/cuts.tsv"
  rm -f "${cuts}"
  if [[ -f "${ROOT_DIR}/tools/stack.toml" ]]; then
    never=(--never "${ROOT_DIR}/tools/stack.toml")
    mkdir -p "$(dirname "${cuts}")"
  fi
  for target in "${TARGETS[@]}"; do
    select_target "${target}"
    if [[ "${BUILD_SYSTEM}" != cmake ]]; then
      echo "stack.sh: ${target} is built by ${BUILD_SYSTEM}; only an AVR that CMake builds has" \
        "the call graphs avr_stack.py adds up."
      continue
    fi
    for variant in "${VARIANTS[@]}"; do
      label="$(target_label "${variant}")"
      if ! target_build "${variant}"; then
        failed=1
        continue
      fi
      # The part, which cmake/toolchains/avr-gcc.cmake keeps in the cache; a toolchain file for
      # another processor has none.
      mcu="$(cmake_cache "${variant}" AVR_MCU)"
      if [[ -z "${mcu}" ]]; then
        echo "stack.sh: ${target} is no AVR, and only an AVR's stack is added up."
        break
      fi
      listed="$(target_programs "${variant}")"
      mapfile -t programs <<< "${listed}"
      for program in "${programs[@]}"; do
        checked=1
        elf="$(target_elf "${variant}" "${program}")"
        listed="$(cmake_graphs "${variant}" "${program}")"
        if [[ -z "${listed}" ]]; then
          err "${program} (${label}): no call graph of it in the build."
          failed=1
          continue
        fi
        mapfile -t graphs <<< "${listed}"
        echo "${program} (${label}):"
        rules=("${never[@]}")
        if ((${#never[@]} > 0)); then
          rules+=(--cuts "${cuts}" --name "${program} (${label})")
        fi
        python3 "${ROOT_DIR}/tools/avr_stack.py" --mcu "${mcu}" "${rules[@]}" "${elf}" \
          "${graphs[@]}" || failed=1
      done
    done
  done
  if ((!checked)); then
    echo "stack.sh: no AVR target built by CMake, nothing to add up."
  elif ((failed)); then
    # What the rules cut in the programs that failed is not known, and would read as a change.
    if ((${#never[@]} > 0)); then
      echo "stack.sh: what the rules of tools/stack.toml cut is checked once every program passes."
    fi
  else
    python3 "${ROOT_DIR}/tools/stack_cuts.py" "${fix[@]}" "${cuts}" || failed=1
  fi
  exit "${failed}"
}

main "$@"
