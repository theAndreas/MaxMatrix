#!/bin/bash
#
# Copyright 2026 Andreas Burnickl
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Checks the style of the C++ sources (clang-format, cpplint, and the indent
# of the lines that continue a Doxygen command, tools/doc_indent.pl), of the
# scripts (shellcheck) and of the CI workflow (actionlint). With --fix, formats
# the C++ sources instead of only checking them. The Python scripts are read
# by ruff. The tools run from the environment uv.lock pins, through
# `uv run --frozen`, so that every machine checks the same; doc_indent.pl
# needs only Perl.

set -euo pipefail

# shellcheck source=tools/common.sh
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

readonly UV_RUN=(uv run --frozen)

main() {
  local fix=0
  if [[ "${1:-}" == "--fix" ]]; then
    fix=1
  elif [[ $# -gt 0 ]]; then
    err "Usage: $0 [--fix]"
    exit 2
  fi

  cd "${ROOT_DIR}"
  # Tracked files and new ones not yet added, but not what .gitignore names:
  # a file is checked before its first commit, not after.
  local files=(git ls-files --cached --others --exclude-standard)
  local sources
  mapfile -t sources < <("${files[@]}" 'src/*' 'examples/*' 'test/*' \
    | grep -E '\.(h|cpp|ino)$')
  local scripts
  mapfile -t scripts < <("${files[@]}" 'tools/*.sh')

  local failed=0
  # The lines that continue a command of a Doxygen block start under its text,
  # docs/Documentation.md. clang-format keeps them so, but a line it breaks
  # starts at the margin again, so the indent comes after it.
  if ((fix)); then
    "${UV_RUN[@]}" clang-format -i "${sources[@]}"
    tools/doc_indent.pl --fix "${sources[@]}"
  else
    "${UV_RUN[@]}" clang-format --dry-run --Werror \
      "${sources[@]}" || failed=1
    tools/doc_indent.pl --check "${sources[@]}" || failed=1
  fi
  "${UV_RUN[@]}" cpplint --quiet "${sources[@]}" || failed=1
  "${UV_RUN[@]}" shellcheck -x "${scripts[@]}" || failed=1
  "${UV_RUN[@]}" actionlint || failed=1
  "${UV_RUN[@]}" ruff check --quiet || failed=1
  exit "${failed}"
}

main "$@"
