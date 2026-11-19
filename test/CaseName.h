// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* The name of a case of a table of a parameterized test, for the tests of the
 * library and of the host code alike. */
#ifndef MAXMATRIX_TEST_CASENAME_H_
#define MAXMATRIX_TEST_CASENAME_H_

#include <gtest/gtest.h>

#include <string>

/* the name of a case of a table, which the table gives as `name` */
template <typename Case>
std::string CaseName(::testing::TestParamInfo<Case> const& info) {
  return info.param.name;
}

#endif  // MAXMATRIX_TEST_CASENAME_H_
