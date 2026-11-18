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

Status Matrix::SetIntensity(uint8_t intensity) {
  if (intensity > max7219::kIntensityMaxValue) {
    return MAXMATRIX_REPORT_ERROR("SetIntensity", Status(Error::kOutOfRange));
  }
  RegisterWrite(max7219::Register::kIntensity, intensity);
  return Error::kOk;
}

void Matrix::SetShutdown(bool shutdown) {
  RegisterWrite(max7219::Register::kShutdown,
                shutdown ? max7219::kShutdownModeShutdownMode
                         : max7219::kShutdownModeNormalOperation);
}

void Matrix::SetDisplayTest(bool display_test) {
  RegisterWrite(max7219::Register::kDisplayTest,
                display_test ? max7219::kDisplayTestDisplayTestMode
                             : max7219::kDisplayTestNormalOperation);
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
  Status status{
      MAXMATRIX_REPORT_ERROR("SetCharWithShift", text_.SetCharWithShift(c))};

  if (status.IsOk()) {
    StartShift(State::kCharShift);
  }
  return status;
}

Status Matrix::SetTextWithShift(span<char const> text, bool repeat) {
  Status status{MAXMATRIX_REPORT_ERROR("SetTextWithShift",
                                       text_.SetTextWithShift(text, repeat))};

  if (status.IsOk()) {
    StartShift(State::kStringShift);
  }
  return status;
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
  Status const status{MAXMATRIX_REPORT_ERROR(
      "Init", panel_.GetWired().GetChain().Init(panel_.GetNumberOfModules()))};
  if (!status.IsOk()) {
    return status;
  }
  state_ = State::kInit;

  /* initialize registers, turn all LEDs off */
  panel_.GetWired().Clear();
  Update();

  SetIntensity(MAXMATRIX_INTENSITY_INIT_VALUE);
  state_ = State::kReady;
  return status;
}

void Matrix::ShiftTask() {
  uint32_t const now{millis()};

  // The interval counts from this step on, not from when the step was due.
  if ((now - last_shift_) < shift_interval_) {
    return;
  }
  last_shift_ = now;

  if ((State::kStringShift == state_) && !text_.StringShiftTask(panel_)) {
    state_ = State::kReady;
  }

  if ((State::kCharShift == state_) && !text_.CharShiftTask(panel_)) {
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

/*
 * Starts shifting a char or a text: sets the state, State::kCharShift
 * or kStringShift, and dates the last step one interval back, so that the first
 * step comes on the next call of ShiftTask() rather than an interval later.
 */
void Matrix::StartShift(State shift_state) {
  state_ = shift_state;
  last_shift_ = millis() - shift_interval_;
}

}  // namespace max_matrix
