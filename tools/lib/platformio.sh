#!/bin/bash
#
# Copyright 2026 Andreas Burnickl
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The build system platformio: a target with BUILD_SYSTEM=platformio is the environment PIO_ENV of
# the platformio.ini in PIO_DIR of tools/project.sh, built by the PlatformIO uv.lock pins. Its
# platform, board and framework, the Arduino core or none, are the platformio.ini's to name, and
# pinned there. tools/common.sh sources this file and calls the functions platformio_*, which
# target_setup() and the others there describe. This file comes from the template.
#
# Of a firmware, the platformio.ini builds the image. Of a library, a project with examples/, it
# builds each example against the library, which it names in lib_deps, so that PlatformIO reads
# the library's library.json as a project of its registry would: one build per example, its
# sketch as the sources of the build.
#
# What the tools need to differ from a plain `pio run` they give it in PlatformIO's own variables,
# so that platformio.ini stays as a developer builds with it:
#
#   PLATFORMIO_BUILD_DIR        <target build dir>/<variant>, one build per variant, and of a
#                               library <target build dir>/<variant>/<example>
#   PLATFORMIO_SRC_DIR          of a library: examples/<example>, the sketch to build
#   PLATFORMIO_BUILD_FLAGS      the variant's flags, as variant_flags() prints them, on every file
#
# A warning fails the build where it lies in a file of the project's own, which the log is read
# for: not -Werror, which a header of a library or of the framework that a source of the project
# includes would fail as well, and that is not the project's to fix.

readonly PIO=(uv run --frozen pio)

# Fails where the selected target names no platformio.ini, and installs its platform and
# packages where it does.
platformio_setup() {
  if [[ -z "${PIO_DIR}" || -z "${PIO_ENV}" ]]; then
    err "Target ${TARGET}: BUILD_SYSTEM=platformio needs PIO_DIR and PIO_ENV in tools/project.sh."
    return 1
  fi
  if [[ ! -f "${ROOT_DIR}/${PIO_DIR}/platformio.ini" ]]; then
    err "Target ${TARGET}: PIO_DIR=${PIO_DIR} holds no platformio.ini."
    return 1
  fi
  download "${PIO[@]}" pkg install --silent -d "${ROOT_DIR}/${PIO_DIR}" -e "${PIO_ENV}"
}

# Whether the project is a library, whose examples PlatformIO builds each, rather than a firmware,
# whose image it builds: a library has examples/.
platformio_library() {
  [[ -d "${ROOT_DIR}/examples" ]]
}

# Prints the build folder of a program in a variant: of an example of a library its own, of the
# image of a firmware the variant's.
# Arguments:
#   variant, program
platformio_dir() {
  local dir
  dir="$(target_build_dir)/$1"
  if platformio_library; then
    dir+="/$2"
  fi
  echo "${dir}"
}

# Runs pio with the arguments given for a program in a variant, with the variables above set, and
# prints what it prints. Its callers test its status, which turns off set -e within it.
# Arguments:
#   variant, program, then what `pio run` is given beyond the folder and the environment
platformio_run() {
  local variant="$1"
  local program="$2"
  shift 2
  local flags
  flags="$(variant_flags "${variant}")" || return
  local dir
  dir="$(platformio_dir "${variant}" "${program}")"
  local sources=()
  if platformio_library; then
    sources=("PLATFORMIO_SRC_DIR=${ROOT_DIR}/examples/${program}")
  fi
  env "${sources[@]}" \
    PLATFORMIO_BUILD_DIR="${dir}" \
    PLATFORMIO_BUILD_FLAGS="${flags//$'\n'/ }" \
    "${PIO[@]}" run -d "${ROOT_DIR}/${PIO_DIR}" -e "${PIO_ENV}" "$@" 2>&1
}

# Prints the warnings of the output given that lie in a file of the project's own: one whose path,
# relative to PIO_DIR as pio prints it, lies in the repository and in no hidden folder of it, as
# .pio/, where PlatformIO keeps the libraries it fetches. The framework's lie outside it.
platformio_own_warnings() {
  local line
  local path
  local inside
  while IFS= read -r line; do
    if [[ ! "${line}" =~ ^([^:[:space:]]+):[0-9]+:([0-9]+:)?\ warning: ]]; then
      continue
    fi
    path="${BASH_REMATCH[1]}"
    if [[ "${path}" != /* ]]; then
      path="${ROOT_DIR}/${PIO_DIR}/${path}"
    fi
    path="$(realpath -m "${path}")"
    if [[ "${path}" != "${ROOT_DIR}"/* ]]; then
      continue
    fi
    inside="/${path#"${ROOT_DIR}"/}"
    if [[ "${inside}" == */.* ]]; then
      continue
    fi
    echo "${line}"
  done <<< "$1"
}

# Builds a program of a variant, failing on an error and on a warning in the project's own files,
# and keeps what pio printed beside it, which platformio_sizes() reads.
# Arguments:
#   variant: one of VARIANTS
#   program: as platformio_programs() prints it
build_program() {
  local variant="$1"
  local program="$2"
  local label
  label="$(target_label "${variant}")"
  if platformio_library; then
    label+=" ${program}"
  fi
  local dir
  dir="$(platformio_dir "${variant}" "${program}")"
  mkdir -p "${dir}"
  rm -f "${dir}/build.log"
  local output
  if ! output="$(platformio_run "${variant}" "${program}")"; then
    err "${label}: the build failed:"
    err "${output}"
    return 1
  fi
  echo "${output}" > "${dir}/build.log"
  local own
  own="$(platformio_own_warnings "${output}")"
  if [[ -n "${own}" ]]; then
    err "${label}: the build warns in the project's own files:"
    err "${own}"
    return 1
  fi
}

# Builds every program of a variant, or the one named, and fails where one fails.
# Arguments:
#   variant: one of VARIANTS
#   program: optional, as platformio_programs() prints it: an example of a library; a firmware
#     has one, the image
platformio_build() {
  local variant="$1"
  local programs=()
  if [[ -n "${2:-}" ]]; then
    programs=("$2")
  else
    local listed
    listed="$(platformio_programs)" || return 1
    mapfile -t programs <<< "${listed}"
  fi
  local failed=0
  local program
  for program in "${programs[@]}"; do
    if ! build_program "${variant}" "${program}"; then
      failed=1
    fi
  done
  return "${failed}"
}

# Prints the name of every program: of a library its examples, as list_examples() of
# tools/lib/arduino_cli.sh names them; of a firmware the image, named as the project is, as the
# folder's, which cpplint requires it to be.
platformio_programs() {
  if platformio_library; then
    list_examples
  else
    basename "${ROOT_DIR}"
  fi
}

# Prints the path of the image's ELF file.
# Arguments:
#   variant, program
platformio_elf() {
  local dir
  dir="$(platformio_dir "$1" "$2")"
  echo "${dir}/${PIO_ENV}/firmware.elf"
}

# Prints flash and RAM of the image as pio reported them, nothing where it was not built.
# Arguments:
#   variant, program
platformio_sizes() {
  local dir
  dir="$(platformio_dir "$1" "$2")"
  if [[ ! -f "${dir}/build.log" ]]; then
    return 0
  fi
  local output
  output="$(< "${dir}/build.log")"
  local flash
  local ram
  flash="$(sed -n 's/^Flash:.*(used \([0-9]*\) bytes.*/\1/p' <<< "${output}")"
  ram="$(sed -n 's/^RAM:.*(used \([0-9]*\) bytes.*/\1/p' <<< "${output}")"
  echo "${flash} ${ram}"
}

# Writes the database of a variant, as `pio run -t compiledb` writes it, rewritten for clang into
# <dir> with tools/lib/clang_database.py, and prints the folder of the one as pio wrote it, which
# names the target's own compiler. pio writes it into PIO_DIR, whatever else it is told, so it is
# moved out of the sources at once. Of a library, pio writes the database of DATABASE_EXAMPLE, and
# tools/lib/sketch_database.py writes its flags for each source of src/ and of the examples the
# target compiles, as arduino_cli_database() of tools/lib/arduino_cli.sh does.
# Arguments:
#   variant: one of TIDY_VARIANTS
#   dir: the folder to write into
platformio_database() {
  local variant="$1"
  local dir="$2"
  local program
  if platformio_library; then
    program="${DATABASE_EXAMPLE}"
  else
    program="$(platformio_programs)"
  fi
  local output
  if ! output="$(platformio_run "${variant}" "${program}" -t compiledb)"; then
    err "${output}"
    return 1
  fi
  local compiler_dir
  compiler_dir="$(platformio_dir "${variant}" "${program}")/compiledb"
  mkdir -p "${compiler_dir}"
  mv "${ROOT_DIR}/${PIO_DIR}/compile_commands.json" "${compiler_dir}/compile_commands.json"
  if platformio_library; then
    local files
    list_files files '\.(cpp|ino)$' 'src/*' 'examples/*'
    local sources
    target_files sources "${files[@]}"
    python3 "${ROOT_DIR}/tools/lib/sketch_database.py" "${compiler_dir}" "${ROOT_DIR}" \
      "${sources[@]/#/${ROOT_DIR}/}" || return
  fi
  python3 "${ROOT_DIR}/tools/lib/clang_database.py" "${compiler_dir}" "${dir}" || return
  echo "${compiler_dir}"
}
