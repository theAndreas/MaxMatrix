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

MaxMatrix::MaxMatrix(byte data_in_pin, byte chip_select_pin, byte clock_pin,
                     byte* buffer, byte number_of_modules)
    : panel_(data_in_pin, chip_select_pin, clock_pin, buffer,
             number_of_modules) {
  state_ = MaxMatrixState::kNone;
  auto_update_ = true;
  shift_interval_ = MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE;
  last_shift_ = 0;
}

MaxMatrix::~MaxMatrix() {}

void MaxMatrix::Init() {
  state_ = MaxMatrixState::kInit;

  panel_.GetWired().GetChain().Init();

  /* initialize registers, turn all LEDs off */
  panel_.GetWired().Clear();
  Update();

  SetIntensity(MAXMATRIX_INTENSITY_INIT_VALUE);
  state_ = MaxMatrixState::kReady;
}

void MaxMatrix::ShiftTask() {
  uint32_t const now = millis();

  // The interval counts from this step on, not from when the step was due.
  if (now - last_shift_ < shift_interval_) return;
  last_shift_ = now;

  if (MaxMatrixState::kStringShift == state_) {
    if (!text_.StringShiftTask(panel_)) state_ = MaxMatrixState::kReady;
  }
  if (MaxMatrixState::kCharShift == state_) {
    if (!text_.CharShiftTask(panel_)) state_ = MaxMatrixState::kReady;
  }
  UpdateIfAuto();
}

void MaxMatrix::SetAutoUpdate(bool auto_update) {
  auto_update_ = auto_update;
  UpdateIfAuto();
}

MaxMatrixStatus MaxMatrix::SetIntensity(byte intensity) {
  if (intensity > MAX7219_REG_INTENSITY_MAX_VALUE)
    return MAXMATRIX_REPORT_ERROR("SetIntensity",
                                  MaxMatrixStatus(MaxMatrixError::kOutOfRange));
  RegisterWrite(MAX7219_REG_INTENSITY_ADDRESS, intensity);
  return MaxMatrixError::kOk;
}

void MaxMatrix::SetShutdown(bool shutdown) {
  RegisterWrite(MAX7219_REG_SHUTDOWN_ADDRESS,
                shutdown ? MAX7219_REG_SHUTDOWN_MODE_SHUTDOWN_MODE
                         : MAX7219_REG_SHUTDOWN_MODE_NORMAL_OPERATION);
}

void MaxMatrix::SetDisplayTest(bool display_test) {
  RegisterWrite(MAX7219_REG_DISPLAY_TEST_ADDRESS,
                display_test ? MAX7219_REG_DISPLAY_TEST_DISPLAY_TEST_MODE
                             : MAX7219_REG_DISPLAY_TEST_NORMAL_OPERATION);
}

MaxMatrixStatus MaxMatrix::SetCharWithShift(char c) {
  MaxMatrixStatus status =
      MAXMATRIX_REPORT_ERROR("SetCharWithShift", text_.SetCharWithShift(c));

  if (status.IsOk()) StartShift(MaxMatrixState::kCharShift);
  return status;
}

MaxMatrixStatus MaxMatrix::SetTextWithShift(char* text, bool repeat) {
  MaxMatrixStatus status = MAXMATRIX_REPORT_ERROR(
      "SetTextWithShift", text_.SetTextWithShift(text, repeat));

  if (status.IsOk()) StartShift(MaxMatrixState::kStringShift);
  return status;
}

void MaxMatrix::StopShift() {
  // Only a shift ends here: before Init() the state says the chain is not set
  // up.
  if (state_ == MaxMatrixState::kStringShift ||
      state_ == MaxMatrixState::kCharShift)
    state_ = MaxMatrixState::kReady;
}

/*
 * Starts shifting a char or a text: sets the state, MaxMatrixState::kCharShift
 * or kStringShift, and dates the last step one interval back, so that the first
 * step comes on the next call of ShiftTask() rather than an interval later.
 */
void MaxMatrix::StartShift(MaxMatrixState shift_state) {
  state_ = shift_state;
  last_shift_ = millis() - shift_interval_;
}
