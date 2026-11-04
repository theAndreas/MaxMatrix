// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The matrix as it is wired.
 *
 * The matrix buffer and what sends it: columns as the MAX7219 drive them, without any orientation.
 * Writing only changes the buffer; Send() brings the changes to the chain.
 */

#ifndef MAXMATRIX_SRC_MATRIX_MAXMATRIXWIRED_H_
#define MAXMATRIX_SRC_MATRIX_MAXMATRIXWIRED_H_

#include "Arduino.h"
#include "MaxMatrixConfig.h"
#include "Max7219/MaxMatrixChain.h"

/* A frame carries one register of every module, so the same column of all modules goes out in one frame. The buffer
 * therefore keeps which columns of a module have changed, as one bit each in changed_columns_, and Send() needs one frame
 * per changed column: eight at most for a whole picture, however many calls drew it. */
class MaxMatrixWired
{
  private:
    MaxMatrixChain chain_;
    byte matrix_buffer_[MAXMATRIX_BUFFER_SIZE];
    byte changed_columns_;

  public:
    MaxMatrixWired(byte, byte, byte);

    // get methods
    MaxMatrixChain& GetChain() { return chain_; }
    MaxMatrixChain const& GetChain() const { return chain_; }
    /* the buffer alone, unchecked: for MaxMatrixPanel, whose coordinates are checked and mapped already */
    byte GetColumn(byte column) { return matrix_buffer_[column]; }

    // set methods
    void StoreBit(byte, byte, bool);

    // methods
    void Clear();
    void Send();
};

#endif
