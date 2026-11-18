// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Holding the sending of a Matrix back for a block.
 */

#ifndef MAXMATRIX_SRC_MAXMATRIXBATCH_H_
#define MAXMATRIX_SRC_MAXMATRIXBATCH_H_

#include "Utility/reference_wrapper.h"

namespace max_matrix {

class Matrix;

/*!
 * \brief Holds the sending back for as long as it exists, as `std::lock_guard`
 *        holds a mutex.
 *
 * The constructor turns auto update off, the destructor puts back what was
 * there before. So the picture is sent at the end of the block, also on a
 * `return` or `break` out of it, and of nested ones only the outermost sends.
 *
 * \code{.cpp}
 * {
 *     max_matrix::Batch batch(matrix);
 *     matrix.Clear();
 *     matrix.SetText("Hi");
 * }  // sent here, at once
 * \endcode
 *
 * \warning It has to be a named variable: an unnamed `Batch{matrix};` is
 *          destroyed at once and holds nothing back.
 *
 * A picture built over several calls of `loop()` does not fit into a block;
 * that is what Matrix::SetAutoUpdate() and Matrix::Update() are for.
 */
class Batch {
 public:
  /*!
   * \brief Turns auto update of `matrix` off until the end of the block.
   *
   * \param[in,out] matrix The matrix; it has to outlive the batch.
   */
  explicit Batch(Matrix& matrix);

  /*! \brief Puts auto update back as it was, which sends the picture if it was
   * on. */
  ~Batch();

  /*! \brief No batch without the matrix it holds back. */
  Batch() = delete;

  /*! \brief Not copyable: a copy would put auto update back twice. */
  Batch(Batch const&) = delete;

  /*! \brief Not assignable, as it is not copyable. */
  Batch& operator=(Batch const&) = delete;

  /*! \brief Not movable: the moved one would put auto update back as well. */
  Batch(Batch&&) = delete;

  /*! \brief Not movable, as it is not copyable. */
  Batch& operator=(Batch&&) = delete;

 private:
  internal::reference_wrapper<Matrix> matrix_;
  bool previous_auto_update_;
};

}  // namespace max_matrix

#endif  // MAXMATRIX_SRC_MAXMATRIXBATCH_H_
