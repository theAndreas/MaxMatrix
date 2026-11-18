// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Holding the sending of a Matrix back for a block.
 */

#include "MaxMatrixBatch.h"

#include "MaxMatrix.h"

namespace max_matrix {

Batch::Batch(Matrix& matrix)
    : matrix_{matrix}, previous_auto_update_{matrix.GetAutoUpdate()} {
  matrix.SetAutoUpdate(false);
}

Batch::~Batch() { matrix_.get().SetAutoUpdate(previous_auto_update_); }

}  // namespace max_matrix
