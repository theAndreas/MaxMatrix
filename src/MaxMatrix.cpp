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

namespace max_matrix {

Matrix::Matrix(byte data_in_pin, byte chip_select_pin, byte clock_pin,
               byte* buffer, byte number_of_modules)
    : panel_(data_in_pin, chip_select_pin, clock_pin, buffer,
             number_of_modules) {
  state_ = State::kNone;
  auto_update_ = true;
  shift_interval_ = MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE;
  last_shift_ = 0;
}

Matrix::~Matrix() {}

void Matrix::Init() {
  state_ = State::kInit;

  panel_.GetWired().GetChain().Init();

  /* initialize registers, turn all LEDs off */
  panel_.GetWired().Clear();
  Update();

  SetIntensity(MAXMATRIX_INTENSITY_INIT_VALUE);
  state_ = State::kReady;
}

void Matrix::ShiftTask() {
  uint32_t const now = millis();

  // The interval counts from this step on, not from when the step was due.
  if (now - last_shift_ < shift_interval_) return;
  last_shift_ = now;

  if (State::kStringShift == state_) {
    if (!text_.StringShiftTask(panel_)) state_ = State::kReady;
  }
  if (State::kCharShift == state_) {
    if (!text_.CharShiftTask(panel_)) state_ = State::kReady;
  }
  UpdateIfAuto();
}

void Matrix::SetAutoUpdate(bool auto_update) {
  auto_update_ = auto_update;
  UpdateIfAuto();
}

Status Matrix::SetIntensity(byte intensity) {
  if (intensity > max7219::kIntensityMaxValue)
    return MAXMATRIX_REPORT_ERROR("SetIntensity", Status(Error::kOutOfRange));
  RegisterWrite(max7219::kIntensityAddress, intensity);
  return Error::kOk;
}

void Matrix::SetShutdown(bool shutdown) {
  RegisterWrite(max7219::kShutdownAddress,
                shutdown ? max7219::kShutdownModeShutdownMode
                         : max7219::kShutdownModeNormalOperation);
}

void Matrix::SetDisplayTest(bool display_test) {
  RegisterWrite(max7219::kDisplayTestAddress,
                display_test ? max7219::kDisplayTestDisplayTestMode
                             : max7219::kDisplayTestNormalOperation);
}

Status Matrix::SetCharWithShift(char c) {
  Status status =
      MAXMATRIX_REPORT_ERROR("SetCharWithShift", text_.SetCharWithShift(c));

  if (status.IsOk()) StartShift(State::kCharShift);
  return status;
}

Status Matrix::SetTextWithShift(char* text, bool repeat) {
  Status status = MAXMATRIX_REPORT_ERROR("SetTextWithShift",
                                         text_.SetTextWithShift(text, repeat));

  if (status.IsOk()) StartShift(State::kStringShift);
  return status;
}

void Matrix::StopShift() {
  // Only a shift ends here: before Init() the state says the chain is not set
  // up.
  if (state_ == State::kStringShift || state_ == State::kCharShift)
    state_ = State::kReady;
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
