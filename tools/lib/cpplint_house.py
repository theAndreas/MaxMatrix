#!/usr/bin/env python3
# Copyright 2026 Andreas Burnickl
# SPDX-License-Identifier: GPL-3.0-or-later
"""Runs cpplint with the checks of our own of clang-tidy, house-*, as categories it knows.

A NOLINT names the check it silences, `// NOLINT(house-braced-var-init): T-3, ...`, and cpplint
reads every NOLINT: a category of its own it applies, one of clang-tidy's it lets pass, as it
knows their prefixes, and any other it reports as readability/nolint, "Unknown NOLINT error
category". It does not know house-, so it reported each NOLINT of a check of our own; turning
readability/nolint off would let a misspelled category of cpplint's own pass as well. So house-
joins the prefixes it lets pass, and cpplint runs as it would.

The list is cpplint's own, not an option of it: if a version of cpplint names it otherwise,
this fails, and tools/lint.sh with it, rather than letting every NOLINT pass.

Usage:
    tools/lib/cpplint_house.py [CPPLINT ARGUMENT...]
"""

import sys

import cpplint

cpplint._OTHER_NOLINT_CATEGORY_PREFIXES.append('house-')
sys.exit(cpplint.main())
