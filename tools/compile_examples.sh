#!/bin/bash
#
# Copyright 2026 Andreas Burnickl
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Compiles every example of a library, or the image of a firmware, for each target in each variant
# and prints flash and RAM of each, as a Markdown table: a column per variant of each target of
# SIZES_TARGETS, all of TARGETS if it names none, named target/variant where the table shows more
# than one target. A target the table leaves out is built and checked all the same.
# Fails where an example does not compile or warns outside the toolchain's own files, where a
# build that passed left no ELF file of one, and where check_elf() of tools/project.sh fails on
# what was built. When every example compiles, the table goes to build/sizes.md as well, which
# tools/documented_sizes.py compares with the README's (`make sizes`); else that file is removed,
# so that no table of an older build is taken for this one's.

set -euo pipefail

# shellcheck source=tools/common.sh
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

main() {
  if [[ $# -gt 0 ]]; then
    err "Usage: $0"
    exit 2
  fi

  local failed=0
  local target
  local program
  local variant
  local label
  local column
  local shown
  local measured
  local built
  local elf
  local listed
  local programs
  local -A flash_size
  local -A ram_size
  local columns=()
  # Each program once, in the order the first target and variant of the table named it.
  local rows=()
  local -r sizes="${BUILD_DIR}/sizes.md"
  rm -f "${sizes}"

  # The targets of the table: a misspelt one would drop its columns without a word.
  local table_targets=("${TARGETS[@]}")
  if ((${#SIZES_TARGETS[@]} > 0)); then
    for target in "${SIZES_TARGETS[@]}"; do
      if [[ " ${TARGETS[*]} " != *" ${target} "* ]]; then
        err "SIZES_TARGETS in tools/project.sh names ${target}, which is no target of TARGETS:" \
          "${TARGETS[*]}."
        exit 1
      fi
    done
    table_targets=("${SIZES_TARGETS[@]}")
  fi

  for target in "${TARGETS[@]}"; do
    select_target "${target}"
    shown=0
    if [[ " ${table_targets[*]} " == *" ${target} "* ]]; then
      shown=1
    fi
    for variant in "${VARIANTS[@]}"; do
      label="$(target_label "${variant}")"
      # The variant alone where the table shows one target, so that it reads as a project's of
      # one target, whatever else is built.
      column="${variant}"
      if ((${#table_targets[@]} > 1)); then
        column="${target}/${variant}"
      fi
      if ((shown)); then
        columns+=("${column}")
      fi
      built=1
      if ! target_build "${variant}"; then
        built=0
        failed=1
      fi
      if ! listed="$(target_programs "${variant}")"; then
        failed=1
        continue
      fi
      mapfile -t programs <<< "${listed}"
      for program in "${programs[@]}"; do
        if ((shown)) && [[ " ${rows[*]} " != *" ${program} "* ]]; then
          rows+=("${program}")
        fi
        elf="$(target_elf "${variant}" "${program}")"
        if [[ ! -f "${elf}" ]]; then
          if ((built)); then
            err "${program} (${label}): built, but there is no ${elf}."
            failed=1
          fi
          continue
        fi
        check_elf "${variant}" "${elf}" || failed=1
        if ((shown)); then
          measured="$(target_sizes "${variant}" "${program}")"
          flash_size["${program}/${column}"]="${measured% *}"
          ram_size["${program}/${column}"]="${measured#* }"
        fi
      done
    done
  done

  # Example, as a library's table has always been headed, where there are examples; else what
  # is built is a firmware's image.
  local header="| Example |"
  if [[ ! -d "${ROOT_DIR}/examples" ]]; then
    header="| Image |"
  fi
  local rule="|---|"
  for column in "${columns[@]}"; do
    header+=" Flash, ${column} |"
    rule+="---|"
  done
  for column in "${columns[@]}"; do
    header+=" RAM, ${column} |"
    rule+="---|"
  done
  local table="${header}"$'\n'"${rule}"
  local row
  for program in "${rows[@]}"; do
    row="| ${program} |"
    for column in "${columns[@]}"; do
      row+=" ${flash_size["${program}/${column}"]:-} |"
    done
    for column in "${columns[@]}"; do
      row+=" ${ram_size["${program}/${column}"]:-} |"
    done
    table+=$'\n'"${row}"
  done
  echo "${table}"
  if ((failed == 0)); then
    mkdir -p "${BUILD_DIR}"
    echo "${table}" > "${sizes}"
  fi
  exit "${failed}"
}

main "$@"
