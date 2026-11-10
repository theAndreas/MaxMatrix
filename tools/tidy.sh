#!/bin/bash
#
# Copyright 2026 Andreas Burnickl
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs clang-tidy on the library and the examples, with the flags arduino-cli compiles them with
# for the board, once for each of TIDY_VARIANTS in tools/project.sh, and on the tests on the
# host; the checks are in .clang-tidy. With --database-only it writes the compilation database
# of the first variant and stops: clangd reads it for the examples, see .clangd. clang-tidy
# runs from the environment uv.lock pins, like the tools of lint.sh.
#
# arduino-cli compiles copies of a sketch's files in its build folder, and a finding there would
# point to the copy. So the flags are taken from its compilation database, and a database of our
# own names the files in the repository; an .ino is compiled as C++ with Arduino.h included
# first, which is what arduino-cli adds to it.
#
# The library is read with the board's flags, not only through the host's database: an int has
# 16 bits on the AVR and 32 on the host, and a check that only saw the host would never meet the
# code as the board does.
#
# The checks of our own, house-*, are a module of clang-tidy's that tools/tidy_module.sh builds
# from tools/tidy_module/ and clang-tidy loads; the database alone does not need it.

set -euo pipefail

# shellcheck source=tools/common.sh
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

readonly CLANG_TIDY=(uv run --frozen clang-tidy)
readonly TIDY_DIR="${BUILD_DIR}/tidy"

# Prints, as a JSON array, the arguments arduino-cli compiles a file of the sketch
# DATABASE_EXAMPLE with, without the file, its output and -w, and changed for clang:
# - The include folders outside the repository, the core's and the board's, become -isystem,
#   so that nothing is reported from headers we do not own.
# - -Wall -Wextra, so that clang's own warnings are reported as well.
# - -Wno-unknown-attributes: clang does not know avr-gcc's attribute __progmem__, which
#   PROGMEM stands for, and takes the tables as in RAM. That changes nothing clang-tidy checks.
arduino_flags() {
  arduino-cli compile --fqbn "${FQBN}" --library "${ROOT_DIR}" \
    --only-compilation-database --build-path "${TIDY_DIR}/arduino" \
    "${ROOT_DIR}/examples/${DATABASE_EXAMPLE}" >/dev/null
  jq --arg root "${ROOT_DIR}" '[.[] | select(.file | endswith(".ino.cpp"))][0]
    | .file as $file
    | .arguments
    | del(.[index("-o"), index("-o") + 1])
    | map(select(. != $file and . != "-w"))
    | map(if startswith("-I") and (startswith("-I" + $root) | not)
          then "-isystem" + .[2:] else . end)
    + ["-Wall", "-Wextra", "-Wno-unknown-attributes"]' \
    "${TIDY_DIR}/arduino/compile_commands.json"
}

# Prints, as a JSON array, the defines of a variant: what clang-tidy can read of its flags.
# A plugin or an optimization of avr-gcc means nothing to clang.
variant_defines() {
  local variant="$1"
  variant_flags "${variant}" | { grep -E '^-D' || true; } | jq -R . | jq -s .
}

# Writes <dir>/compile_commands.json for the files given, with the flags of arduino_flags() and
# the defines in `defines`, a JSON array.
# Arguments:
#   dir, flags, defines, then the files
write_database() {
  local dir="$1"
  local flags="$2"
  local defines="$3"
  shift 3
  mkdir -p "${dir}"
  printf '%s\n' "$@" | jq -R . | jq -s \
    --arg dir "${ROOT_DIR}" --argjson flags "${flags}" \
    --argjson defines "${defines}" '
    map({
      directory: $dir,
      file: .,
      arguments: ($flags + $defines
        + (if endswith(".ino") then ["-x", "c++", "-include", "Arduino.h"]
           else [] end)
        + [.])
    })' > "${dir}/compile_commands.json"
}

main() {
  local database_only=0
  if [[ "${1:-}" == "--database-only" ]]; then
    database_only=1
  elif [[ $# -gt 0 ]]; then
    err "Usage: $0 [--database-only]"
    exit 2
  fi

  cd "${ROOT_DIR}"
  mkdir -p "${TIDY_DIR}"
  local sources
  mapfile -t sources < <(git ls-files --cached --others --exclude-standard \
    'src/*' 'examples/*' | grep -E '\.(cpp|ino)$')
  local flags
  flags="$(arduino_flags)"
  local clang_tidy=("${CLANG_TIDY[@]}")
  if ((!database_only)); then
    local module
    module="$(tools/tidy_module.sh)"
    clang_tidy+=("--load=${module}")
  fi

  # Each variant takes other branches of #if. The first is the one clangd reads, see .clangd.
  local failed=0
  local variant
  for variant in "${TIDY_VARIANTS[@]}"; do
    write_database "${TIDY_DIR}/${variant}" "${flags}" \
      "$(variant_defines "${variant}")" "${sources[@]}"
    if ((database_only)); then
      echo "Wrote ${TIDY_DIR}/${variant}/compile_commands.json"
      exit 0
    fi
    echo "clang-tidy, ${variant}:"
    "${clang_tidy[@]}" -p "${TIDY_DIR}/${variant}" --quiet \
      "${sources[@]}" || failed=1
  done

  # The tests on the host, against the mock, with the database CMake writes. Only what lies in
  # test/: the tests are C++17, and the headers of the library, checked above as the gnu++11
  # they are, would be held to C++17. Not the mock in test/mock/, which stands in for the
  # Arduino core and follows it, not the Google style. Nor what lies in test/compile_fail/,
  # which must not compile and is in no database.
  local tests
  mapfile -t tests < <(git ls-files --cached --others --exclude-standard \
    'test/*' ':!test/mock/*' ':!test/compile_fail/*' | grep -E '\.cpp$')
  cmake --preset default >/dev/null
  echo "clang-tidy, the tests on the host:"
  "${clang_tidy[@]}" -p "${BUILD_DIR}/host" --quiet \
    --header-filter='(^|/)test/' \
    "${tests[@]}" || failed=1
  exit "${failed}"
}

main "$@"
