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

/*!
 * \brief The matrix as it is wired: the buffer of what the LEDs show, and what of it changed.
 *
 * The buffer mirrors the LEDs, one byte per column, columns of all modules in a row, module 0
 * first. Drawing only changes the buffer; Send() brings what changed to the chain.
 */
class MaxMatrixWired
{
  private:
    MaxMatrixChain chain_;
    byte matrix_buffer_[MAXMATRIX_BUFFER_SIZE];
    /*
     * A frame carries one register of every module, so the same column of all modules goes out in
     * one frame. The buffer therefore keeps which columns of a module have changed, one bit each,
     * and Send() needs one frame per changed column: eight at most for a whole picture, however
     * many calls drew it.
     */
    byte changed_columns_;

  public:
    /*!
     * \brief Makes the matrix of the chain at these pins, with an empty buffer.
     *
     * \param[in] data_in_pin The pin DIN of the chain is connected to.
     * \param[in] chip_select_pin The pin CS of the chain is connected to.
     * \param[in] clock_pin The pin CLK of the chain is connected to.
     */
    MaxMatrixWired(byte data_in_pin, byte chip_select_pin, byte clock_pin);

    /*! \brief Returns the chain the matrix sends to. */
    MaxMatrixChain& GetChain() { return chain_; }

    /*! \copydoc GetChain() */
    MaxMatrixChain const& GetChain() const { return chain_; }

    /*!
     * \brief Returns a column of the buffer, unchecked.
     *
     * For MaxMatrixPanel, whose coordinates are checked and mapped already.
     *
     * \param[in] column The column in the buffer.
     */
    byte GetColumn(byte column) { return matrix_buffer_[column]; }

    /*!
     * \brief Sets one bit of the buffer.
     *
     * Send() brings the change to the chain. The column is marked as changed only when its value
     * changes, so that drawing what is shown already sends nothing.
     *
     * \param[in] column The column in the buffer.
     * \param[in] bit The bit in the column, the row as wired.
     * \param[in] value True for a lit LED.
     * \pre `column` lies within the buffer and `bit` within a byte: MaxMatrixPanel has checked and
     *      mapped them.
     */
    void StoreBit(byte column, byte bit, bool value);

    /*!
     * \brief Turns every LED off in the buffer, and marks every column as changed.
     *
     * Even a column that was empty already is marked: after power-up the registers of the MAX7219
     * hold anything, and MaxMatrix::Init() clears them this way.
     */
    void Clear();

    /*!
     * \brief Sends what has changed.
     *
     * Every changed column of a module goes out in one frame for all modules; nothing goes out when
     * nothing has changed.
     */
    void Send();
};

#endif
