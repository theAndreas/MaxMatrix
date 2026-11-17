// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief A whole file read into a string: the page the web host serves, and the
 *        panel a run with `--expect` compares with.
 */

#ifndef MAXMATRIX_SIMULATOR_HOST_SIMULATORFILE_H_
#define MAXMATRIX_SIMULATOR_HOST_SIMULATORFILE_H_

#include <string>

#include "MaxMatrixAttributes.h"

/*! \brief A file's text as SimulatorFile::Read() gives it. */
struct SimulatorFileText {
  std::string text; /*!< What was read; all of the file if `complete`. */
  bool complete;    /*!< Whether the file was read to its end. */
};

/*!
 * \brief A whole file read into a string.
 *
 * It has no state of its own, so its function is static, as in
 * max_matrix::Font.
 */
class SimulatorFile {
 public:
  /*!
   * \brief Reads the whole file at `path`.
   *
   * \param[in] path The file.
   * \return Its text, and whether it was read to its end; of a file that cannot
   *         be read, what was read even so, for the caller to show.
   */
  MAXMATRIX_NODISCARD static SimulatorFileText Read(char const* path);
};

#endif  // MAXMATRIX_SIMULATOR_HOST_SIMULATORFILE_H_
