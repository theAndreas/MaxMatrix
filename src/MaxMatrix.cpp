// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief What the facade does itself.
 *
 * Setting the whole chain up, the state of the library, which the shifting text
 * only reports into, and when what was drawn is sent.
 */

#include "MaxMatrix.h"

#include <stdint.h>

namespace max_matrix {

Matrix::Matrix(DataInPin data_in_pin, ChipSelectPin chip_select_pin,
               ClockPin clock_pin, span<Byte> buffer)
    : panel_{data_in_pin, chip_select_pin, clock_pin, buffer} {}

void Matrix::SetAutoUpdate(bool auto_update) {
  auto_update_ = auto_update;
  UpdateIfAuto();
}

// The settings are sent at once and not kept: before Init() nothing is sent,
// and Init() then sends its own, so that one set before would be lost unseen.
Status Matrix::SetIntensity(uint8_t intensity) {
  Status status{Error::kNotSetUp};
  if (intensity > max7219::kIntensityMaxValue) {
    status = Error::kOutOfRange;
  } else if (state_ != State::kNone) {
    RegisterWrite(max7219::Register::kIntensity, intensity);
    status = Error::kOk;
  } else {
    // not set up: nothing is sent, and the status says so
  }
  return MAXMATRIX_REPORT_ERROR("SetIntensity", status);
}

Status Matrix::SetShutdown(bool shutdown) {
  Status status{Error::kNotSetUp};
  if (state_ != State::kNone) {
    RegisterWrite(max7219::Register::kShutdown,
                  shutdown ? max7219::kShutdownModeShutdownMode
                           : max7219::kShutdownModeNormalOperation);
    status = Error::kOk;
  }
  return MAXMATRIX_REPORT_ERROR("SetShutdown", status);
}

Status Matrix::SetDisplayTest(bool display_test) {
  Status status{Error::kNotSetUp};
  if (state_ != State::kNone) {
    RegisterWrite(max7219::Register::kDisplayTest,
                  display_test ? max7219::kDisplayTestDisplayTestMode
                               : max7219::kDisplayTestNormalOperation);
    status = Error::kOk;
  }
  return MAXMATRIX_REPORT_ERROR("SetDisplayTest", status);
}

void Matrix::SetColumn(X x, Byte value) {
  panel_.SetColumn(x, value);
  UpdateIfAuto();
}

void Matrix::SetColumn(Module module, uint8_t column, Byte value) {
  panel_.SetColumn(module, column, value);
  UpdateIfAuto();
}

void Matrix::SetRow(Y y, span<Byte const> values) {
  panel_.SetRow(y, values);
  UpdateIfAuto();
}

void Matrix::SetRow(Module module, uint8_t row, Byte value) {
  panel_.SetRow(module, row, value);
  UpdateIfAuto();
}

void Matrix::SetDot(Point dot, bool value) {
  panel_.SetDot(dot, value);
  UpdateIfAuto();
}

void Matrix::DrawRect(Point corner, Point opposite_corner, bool value) {
  panel_.DrawRect(corner, opposite_corner, value);
  UpdateIfAuto();
}

void Matrix::FillRect(Point corner, Point opposite_corner, bool value) {
  panel_.FillRect(corner, opposite_corner, value);
  UpdateIfAuto();
}

void Matrix::DrawCircle(Point centre, int16_t radius, bool value) {
  panel_.DrawCircle(centre, radius, value);
  UpdateIfAuto();
}

void Matrix::DrawBitmap(Point top_left, uint8_t height,
                        span<uint8_t const> bitmap) {
  panel_.DrawBitmap(top_left, height, bitmap);
  UpdateIfAuto();
}

Status Matrix::SetCharWithShift(char c) {
  // Before Init() the state says that the chain is not set up; a shift would
  // overwrite it, and Init() end the shift unseen.
  Status status{Error::kNotSetUp};
  if (state_ != State::kNone) {
    status = text_.SetCharWithShift(c);
  }
  if (status.IsOk()) {
    state_ = State::kCharShift;
  }
  return MAXMATRIX_REPORT_ERROR("SetCharWithShift", status);
}

Status Matrix::SetTextWithShift(span<char const> text, bool repeat) {
  // As in SetCharWithShift().
  Status status{Error::kNotSetUp};
  if (state_ != State::kNone) {
    status = text_.SetTextWithShift(text, repeat);
  }
  if (status.IsOk()) {
    state_ = State::kStringShift;
  }
  return MAXMATRIX_REPORT_ERROR("SetTextWithShift", status);
}

Status Matrix::SetTextWithShift(__FlashStringHelper const* text, bool repeat) {
  // As in SetCharWithShift().
  Status status{Error::kNotSetUp};
  if (state_ != State::kNone) {
    status = text_.SetTextWithShift(text, repeat);
  }
  if (status.IsOk()) {
    state_ = State::kStringShift;
  }
  return MAXMATRIX_REPORT_ERROR("SetTextWithShift", status);
}

void Matrix::StopShift() {
  // Only a shift ends here: before Init() the state says the chain is not set
  // up.
  if ((state_ == State::kStringShift) || (state_ == State::kCharShift)) {
    state_ = State::kReady;
  }
}

void Matrix::SetSprite(Point top_left, Sprite const& sprite) {
  max_matrix::internal::Text::SetSprite(panel_, top_left, sprite);
  UpdateIfAuto();
}

void Matrix::SetModuleType(ModuleType module_type) {
  panel_.SetModuleType(module_type);
  UpdateIfAuto();
}

void Matrix::SetMatrixOrientation(MatrixOrientation matrix_orientation) {
  panel_.SetMatrixOrientation(matrix_orientation);
  UpdateIfAuto();
}

Status Matrix::Init() {
  // A buffer that is no chain would wrap the count of the columns or leave
  // columns no module has.
  Status chain_status{Error::kOutOfRange};
  if (panel_.GetWired().HoldsAChain()) {
    chain_status =
        panel_.GetWired().GetChain().Init(panel_.GetNumberOfModules());
  }
  Status const status{MAXMATRIX_REPORT_ERROR("Init", chain_status)};
  if (!status.IsOk()) {
    return status;
  }
  state_ = State::kInit;

  /* initialize registers, turn all LEDs off */
  panel_.GetWired().Clear();
  Update();

  /*
   * The static_assert beside max7219::kIntensityMaxValue holds the value in
   * range. RegisterWrite() rather than SetIntensity(), whose checks and report
   * cost up to 82 bytes of flash with MAXMATRIX_DEV_ERROR_DETECT in a sketch
   * that never sets the intensity, and only Basics, which does, 10 bytes.
   */
  RegisterWrite(max7219::Register::kIntensity, MAXMATRIX_INTENSITY_INIT_VALUE);
  state_ = State::kReady;
  return status;
}

void Matrix::ShiftStep() {
  // The step stands alone rather than behind a && that may skip it, which a
  // reader of the condition misses (MISRA 8.14.1).
  bool ended{false};
  if (State::kStringShift == state_) {
    ended = !text_.StringShiftStep(panel_);
  } else if (State::kCharShift == state_) {
    ended = !text_.CharShiftStep(panel_);
  } else {
    // no shift runs, so there is no step to take
  }
  if (ended) {
    state_ = State::kReady;
  }

  UpdateIfAuto();
}

void Matrix::ShiftLeft(ShiftFill fill) {
  panel_.ShiftLeft(fill);
  UpdateIfAuto();
}

Byte Matrix::ShiftLeft(Byte shifted_in) {
  Byte const shifted_out{panel_.ShiftLeft(shifted_in)};
  UpdateIfAuto();
  return shifted_out;
}

void Matrix::ShiftRight(ShiftFill fill) {
  panel_.ShiftRight(fill);
  UpdateIfAuto();
}

Byte Matrix::ShiftRight(Byte shifted_in) {
  Byte const shifted_out{panel_.ShiftRight(shifted_in)};
  UpdateIfAuto();
  return shifted_out;
}

void Matrix::ShiftUp(ShiftFill fill) {
  panel_.ShiftUp(fill);
  UpdateIfAuto();
}

void Matrix::ShiftDown(ShiftFill fill) {
  panel_.ShiftDown(fill);
  UpdateIfAuto();
}

void Matrix::UpdateIfAuto() {
  if (auto_update_) {
    Update();
  }
}

Status Matrix::UpdateIfAuto(Status status) {
  UpdateIfAuto();
  return status;
}

}  // namespace max_matrix
