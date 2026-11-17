// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief A whole file read into a string.
 */

#include "SimulatorFile.h"

#include <fstream>
#include <sstream>

SimulatorFileText SimulatorFile::Read(char const* path) {
  std::ifstream const file{path};
  std::stringstream content;

  content << file.rdbuf();
  return SimulatorFileText{content.str(), static_cast<bool>(file)};
}
