// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The header a sketch includes: the matrix it draws on.
 *
 * MaxMatrix drives a chain of 8x8 LED modules with the MAX7219. The class MaxMatrix is the facade
 * over the parts in the folders beside this file, one per subject: `Max7219/` the transfer to the
 * chain, `Matrix/` the buffer as wired and the coordinates as the viewer sees them, `Font/` the
 * sprites of the chars, `Text/` the text that stands or shifts, and `Command/` the lines a control
 * page sends over `Serial`, built on this class. A function here that only passes a call on takes
 * its text from the function it calls.
 */

#ifndef MAXMATRIX_SRC_MAXMATRIX_H_
#define MAXMATRIX_SRC_MAXMATRIX_H_

#include "Arduino.h"
#include "MaxMatrixStandardTypes.h"
#include "MaxMatrixError.h"
#include "MaxMatrixConfig.h"
#include "Max7219/MaxMatrixChain.h"
#include "Matrix/MaxMatrixPanel.h"
#include "Font/MaxMatrixFont.h"
#include "Text/MaxMatrixText.h"
#include "Command/MaxMatrixCommand.h"

/*! \brief What a MaxMatrix is doing, as MaxMatrix::GetState() tells it. */
enum class MaxMatrixState : uint8_t {
    kNone,        /*!< Constructed; MaxMatrix::Init() not called yet. */
    kInit,        /*!< MaxMatrix::Init() is running. */
    kReady,       /*!< Nothing to shift: a text or a char shifted in is complete. */
    kStringShift, /*!< A text of MaxMatrix::SetTextWithShift() is being shifted in. */
    kCharShift    /*!< A char of MaxMatrix::SetCharWithShift() is being shifted in. */
};

/*!
 * \brief The matrix a sketch draws on: a chain of 8x8 modules, in the coordinates the viewer sees.
 *
 * Every function that draws changes the buffer and, with auto update on, the default, sends what
 * changed before it returns: one frame per changed column of a module, eight at most, however much
 * the call drew. A sketch that builds one picture out of many calls holds the sending back with a
 * MaxMatrixBatch, or with SetAutoUpdate() and Update(), so that no half of a picture is ever shown.
 * The intensity and the other registers are sent at once either way: they are not part of the
 * picture.
 *
 * A sketch makes it as a MaxMatrixOf, which brings the buffer for its chain:
 *
 * \code{.cpp}
 * MaxMatrixOf<4> matrix(kDataInPin, kChipSelectPin, kClockPin);
 *
 * void setup() {
 *     matrix.Init();
 *     matrix.SetText("Hi!");
 * }
 * \endcode
 *
 * A shifting text keeps its own pace: ShiftTask() may be called on every `loop()` and takes a step
 * only when the shift interval has passed since the last one, so that no sketch has to time it.
 *
 * \note With `MAXMATRIX_DEV_ERROR_DETECT` at `STD_ON`, every function here that fails reports its
 *       name and the error to MaxMatrixReportError().
 */
class MaxMatrix
{
  private:
    MaxMatrixPanel panel_;
    MaxMatrixText text_;
    MaxMatrixState state_;
    bool auto_update_;
    uint16_t shift_interval_;
    uint32_t last_shift_;

    // Sends what changed, if auto update is on; every public function that draws ends with it.
    void UpdateIfAuto() { if(auto_update_) Update(); }
    MaxMatrixStatus UpdateIfAuto(MaxMatrixStatus status) { UpdateIfAuto(); return status; }
    void StartShift(MaxMatrixState shift_state);

  public:
    /*!
     * \brief Makes a matrix on the chain at these pins, in the buffer given, and empties it.
     *
     * Nothing is sent until Init(). MaxMatrixOf brings the buffer itself and checks the number of
     * modules when the sketch is compiled; this is for a chain whose length is known only at run
     * time, or a buffer that lies elsewhere.
     *
     * \code{.cpp}
     * byte buffer[MAXMATRIX_BUFFER_SIZE(4)];
     * MaxMatrix matrix(kDataInPin, kChipSelectPin, kClockPin, buffer, 4);
     * \endcode
     *
     * \param[in] data_in_pin The pin DIN of the chain is connected to.
     * \param[in] chip_select_pin The pin CS of the chain is connected to.
     * \param[in] clock_pin The pin CLK of the chain is connected to.
     * \param[out] buffer The buffer, `MAXMATRIX_BUFFER_SIZE(number_of_modules)` bytes, which has to
     *                    live as long as the matrix.
     * \param[in] number_of_modules The modules in the chain, 1 to
     *                              `MAXMATRIX_MAX_NUMBER_OF_MODULES`.
     */
    MaxMatrix(byte data_in_pin, byte chip_select_pin, byte clock_pin, byte* buffer, byte number_of_modules);

    /*! \brief Leaves the LEDs as they are. */
    ~MaxMatrix();

    /*! \brief Not copyable: the copy would draw into the same buffer. */
    MaxMatrix(MaxMatrix const&) = delete;

    /*! \brief Not assignable, as it is not copyable. */
    MaxMatrix& operator=(MaxMatrix const&) = delete;

    /*! \brief Returns the modules of the chain. */
    byte GetNumberOfModules() const { return panel_.GetNumberOfModules(); }

    /*! \brief Returns the columns of the matrix, eight per module. */
    byte GetNumberOfColumns() const { return panel_.GetNumberOfColumns(); }

    /*! \brief Returns what the matrix is doing; MaxMatrixState::kReady once a shifted text is complete. */
    MaxMatrixState GetState() const { return state_; }

    /*! \brief Returns whether every function that draws sends its result at once; see SetAutoUpdate(). */
    bool GetAutoUpdate() const { return auto_update_; }

    /*! \brief Returns the milliseconds per column of a shifting text; see SetShiftInterval(). */
    uint16_t GetShiftInterval() const { return shift_interval_; }

    /*! \brief Returns the pin DIN of the chain is connected to. */
    byte GetDataInPin() const { return panel_.GetWired().GetChain().GetDataInPin(); }

    /*! \brief Returns the pin CS of the chain is connected to. */
    byte GetChipSelectPin() const { return panel_.GetWired().GetChain().GetChipSelectPin(); }

    /*! \brief Returns the pin CLK of the chain is connected to. */
    byte GetClockPin() const { return panel_.GetWired().GetChain().GetClockPin(); }

    /*! \brief Returns how every module is turned in its place; see SetModuleOrientation(). */
    MaxMatrixModuleOrientation GetModuleOrientation() const { return panel_.GetModuleOrientation(); }

    /*! \brief Returns how the whole matrix is turned; see SetMatrixOrientation(). */
    MaxMatrixMatrixOrientation GetMatrixOrientation() const { return panel_.GetMatrixOrientation(); }

    /*! \brief Returns the empty columns between two chars of a text. */
    byte GetSpaceBetweenChars() const { return text_.GetSpaceBetweenChars(); }

    /*! \copydoc MaxMatrixPanel::GetColumn(int16_t) */
    MaxMatrixResult<byte> GetColumn(int16_t column) { return MAXMATRIX_REPORT_ERROR("GetColumn", panel_.GetColumn(column)); }

    /*! \copydoc MaxMatrixPanel::GetColumn(byte, byte) */
    MaxMatrixResult<byte> GetColumn(byte module, byte column) { return MAXMATRIX_REPORT_ERROR("GetColumn", panel_.GetColumn(module, column)); }

    /*!
     * \brief Gives a row of the matrix, one byte per module.
     *
     * \code{.cpp}
     * byte row[4];
     * matrix.GetRow(0, row);
     * \endcode
     *
     * \tparam NumberOfValues The bytes of `values`, which the array says itself.
     * \param[in] row The row, from 0 at the top.
     * \param[out] values The row, bit 7 of each byte the leftmost column of its module; left as it
     *                    was on an error.
     * \return MaxMatrixError::kOk, or MaxMatrixError::kOutOfRange if the row lies off the panel or
     *         `values` has fewer bytes than there are modules.
     */
    template<size_t NumberOfValues>
    MaxMatrixStatus GetRow(int16_t row, byte (&values)[NumberOfValues])
    {
        return MAXMATRIX_REPORT_ERROR("GetRow", panel_.GetRow(row, values, NumberOfValues > 0xFF ? 0xFF : NumberOfValues));
    }

    /*! \copydoc MaxMatrixPanel::GetRow(byte, byte) */
    MaxMatrixResult<byte> GetRow(byte module, byte row) { return MAXMATRIX_REPORT_ERROR("GetRow", panel_.GetRow(module, row)); }

    /*! \copydoc MaxMatrixPanel::GetDot(int16_t, int16_t) */
    MaxMatrixResult<bool> GetDot(int16_t column, int16_t row) { return MAXMATRIX_REPORT_ERROR("GetDot", panel_.GetDot(column, row)); }

    /*! \copydoc MaxMatrixText::GetFont() */
    uint8_t const* GetFont() const { return text_.GetFont(); }

    /*!
     * \brief Sets the pin DIN of the chain is connected to.
     *
     * \param[in] data_in_pin The pin; Init() makes it an output.
     */
    void SetDataInPin(byte data_in_pin) { panel_.GetWired().GetChain().SetDataInPin(data_in_pin); }

    /*!
     * \brief Sets the pin CS of the chain is connected to.
     *
     * \param[in] chip_select_pin The pin; Init() makes it an output.
     */
    void SetChipSelectPin(byte chip_select_pin) { panel_.GetWired().GetChain().SetChipSelectPin(chip_select_pin); }

    /*!
     * \brief Sets the pin CLK of the chain is connected to.
     *
     * \param[in] clock_pin The pin; Init() makes it an output.
     */
    void SetClockPin(byte clock_pin) { panel_.GetWired().GetChain().SetClockPin(clock_pin); }

    /*!
     * \brief Sets whether what is drawn is sent at once.
     *
     * On, the default, every function that draws sends its result before it returns. Off, only
     * Update() sends. Turning it on sends what was drawn while it was off. A MaxMatrixBatch turns it
     * off for a block and back on at its end.
     *
     * \param[in] auto_update True to send after every function that draws.
     */
    void SetAutoUpdate(bool auto_update);

    /*!
     * \brief Sets the milliseconds per column of a shifting text.
     *
     * \param[in] shift_interval The milliseconds; 0 makes every ShiftTask() a step.
     */
    void SetShiftInterval(uint16_t shift_interval) { shift_interval_ = shift_interval; }

    /*!
     * \brief Sets how bright every LED is lit.
     *
     * Sent at once, whatever auto update is.
     *
     * \param[in] intensity The intensity, 0 to 15.
     * \return MaxMatrixError::kOk, or MaxMatrixError::kOutOfRange above 15; the intensity then
     *         stays as it was.
     */
    MaxMatrixStatus SetIntensity(byte intensity);

    /*!
     * \brief Turns the LEDs of every MAX7219 off, or on again.
     *
     * In shutdown the MAX7219s draw next to no current, keep their registers and still take what
     * is drawn: the LEDs show the picture as it is then once the shutdown ends, at the intensity
     * they had. Sent at once, whatever auto update is.
     *
     * \param[in] shutdown True to turn the LEDs off, false to turn them on again.
     * \pre Init().
     */
    void SetShutdown(bool shutdown);

    /*!
     * \brief Lights every LED at full brightness, or shows the picture again.
     *
     * The display test of the MAX7219 overrides the picture and the intensity without changing
     * either, and also lights a module in shutdown: a check that every LED and every module of
     * the chain works. Sent at once, whatever auto update is.
     *
     * \param[in] display_test True to light every LED, false to show the picture again.
     * \pre Init().
     */
    void SetDisplayTest(bool display_test);

    /*! \copydoc MaxMatrixPanel::SetColumn(int16_t, byte) */
    void SetColumn(int16_t column, byte value) { panel_.SetColumn(column, value); UpdateIfAuto(); }

    /*! \copydoc MaxMatrixPanel::SetColumn(byte, byte, byte) */
    void SetColumn(byte module, byte column, byte value) { panel_.SetColumn(module, column, value); UpdateIfAuto(); }

    /*!
     * \brief Sets a row of the matrix, one byte per module.
     *
     * A row off the panel is left out, as for SetDot(), and so are the modules beyond the bytes
     * given, or the bytes beyond the modules.
     *
     * \tparam NumberOfValues The bytes of `values`, which the array says itself.
     * \param[in] row The row, from 0 at the top.
     * \param[in] values The row, bit 7 of each byte the leftmost column of its module.
     */
    template<size_t NumberOfValues>
    void SetRow(int16_t row, byte const (&values)[NumberOfValues])
    {
        panel_.SetRow(row, values, NumberOfValues > 0xFF ? 0xFF : NumberOfValues);
        UpdateIfAuto();
    }

    /*! \copydoc MaxMatrixPanel::SetRow(byte, byte, byte) */
    void SetRow(byte module, byte row, byte value) { panel_.SetRow(module, row, value); UpdateIfAuto(); }

    /*! \copydoc MaxMatrixPanel::SetDot(int16_t, int16_t, bool) */
    void SetDot(int16_t column, int16_t row, bool value) { panel_.SetDot(column, row, value); UpdateIfAuto(); }

    /*!
     * \brief Draws a char with its top left corner at `x`, `y`.
     *
     * What lies off the matrix is left out.
     *
     * \param[in] x The column of the left edge; it may lie off the matrix.
     * \param[in] y The row of the top edge; it may lie off the matrix.
     * \param[in] c The char, latin-1.
     * \return MaxMatrixError::kOk, or MaxMatrixError::kUnknownChar if the font has no sprite for
     *         the char; nothing is drawn then.
     */
    MaxMatrixStatus SetChar(int16_t x, int16_t y, char c) { return UpdateIfAuto(MAXMATRIX_REPORT_ERROR("SetChar", text_.SetChar(panel_, x, y, c))); }

    /*!
     * \brief Starts shifting a char in from the right, until it stands at the right edge.
     *
     * ShiftTask() takes the steps; GetState() is MaxMatrixState::kCharShift until the char stands.
     *
     * \param[in] c The char, latin-1.
     * \return MaxMatrixError::kOk, or MaxMatrixError::kUnknownChar if the font has no sprite for
     *         the char; nothing starts then.
     */
    MaxMatrixStatus SetCharWithShift(char c);

    /*!
     * \brief Draws a text from the left edge, as far as it fits.
     *
     * \param[in] string The text, UTF-8 or latin-1.
     * \return MaxMatrixError::kOk; MaxMatrixError::kNoText if `string` is null; or
     *         MaxMatrixError::kUnknownChar if a char of the text is not in the font, with the chars
     *         before it drawn.
     */
    MaxMatrixStatus SetText(char const* string) { return UpdateIfAuto(MAXMATRIX_REPORT_ERROR("SetText", text_.SetText(panel_, string))); }

    /*!
     * \brief Starts shifting a text in from the right, until its last char stands at the right
     *        edge.
     *
     * ShiftTask() takes the steps; GetState() is MaxMatrixState::kStringShift until the text
     * stands.
     *
     * \param[in] string The text, UTF-8 or latin-1. It has to live until the text stands.
     * \return MaxMatrixError::kOk, or MaxMatrixError::kNoText if `string` is null; nothing starts
     *         then.
     */
    MaxMatrixStatus SetTextWithShift(char* string);

    /*!
     * \brief Sets the empty columns between two chars of a text.
     *
     * \param[in] space_between_chars The number of empty columns.
     */
    void SetSpaceBetweenChars(byte space_between_chars) { text_.SetSpaceBetweenChars(space_between_chars); }

    /*!
     * \brief Sets the font every char is drawn in from now on.
     *
     * A font of MD_MAX72XX serves as it is, in its array in flash; MaxMatrixFont describes the
     * format. Chars drawn already stay as they are, and a char that shifts in goes on in the font
     * it started in. kMaxMatrixDefaultFont is the font MaxMatrix starts with.
     *
     * \code{.cpp}
     * uint8_t const kBigFont[] PROGMEM = {'F', 1, ...};
     *
     * matrix.SetFont(kBigFont);
     * \endcode
     *
     * \param[in] font The font, in flash; it has to live as long as it is set, as a `PROGMEM` array
     *                 does.
     * \return MaxMatrixError::kOk, or MaxMatrixError::kNotAFont if `font` is null or does not start
     *         as a font does; the font then stays as it was.
     */
    MaxMatrixStatus SetFont(uint8_t const* font) { return MAXMATRIX_REPORT_ERROR("SetFont", text_.SetFont(font)); }

    /*!
     * \brief Draws a sprite with its top left corner at `x`, `y`.
     *
     * What lies off the matrix is left out.
     *
     * \param[in] x The column of the left edge; it may lie off the matrix.
     * \param[in] y The row of the top edge; it may lie off the matrix.
     * \param[in] sprite Its width, its height and up to five columns, bit 0 the top row.
     */
    void SetSprite(int16_t x, int16_t y, MaxMatrixSprite const sprite) { text_.SetSprite(panel_, x, y, sprite); UpdateIfAuto(); }

    /*! \copydoc MaxMatrixPanel::SetModuleOrientation(MaxMatrixModuleOrientation) */
    void SetModuleOrientation(MaxMatrixModuleOrientation module_orientation) { panel_.SetModuleOrientation(module_orientation); UpdateIfAuto(); }

    /*! \copydoc MaxMatrixPanel::SetMatrixOrientation(MaxMatrixMatrixOrientation) */
    void SetMatrixOrientation(MaxMatrixMatrixOrientation matrix_orientation) { panel_.SetMatrixOrientation(matrix_orientation); UpdateIfAuto(); }

    /*!
     * \brief Sets the chain up: the pins, every register of every MAX7219, and every LED off.
     *
     * The intensity is set to its start value. The cleared matrix is sent even with auto update
     * off, as after power-up the LEDs show anything.
     */
    void Init();

    /*!
     * \brief Shifts a text or a char one column further, if the shift interval has passed.
     *
     * Call it on every `loop()` until GetState() is MaxMatrixState::kReady again. It takes a step
     * only when the shift interval has passed since the last one, and with an interval of 0 on
     * every call. A call that comes late takes one step, not the ones it missed: catching up would
     * make the text jump after `loop()` was held up.
     *
     * \pre SetCharWithShift() or SetTextWithShift() started a shift.
     */
    void ShiftTask();

    /*!
     * \brief Sends what was drawn since the last update.
     *
     * Only needed with auto update off; see MaxMatrixBatch.
     */
    void Update() { panel_.GetWired().Send(); }

    /*! \brief Turns every LED off. */
    void Clear() { panel_.GetWired().Clear(); UpdateIfAuto(); }

    /*!
     * \brief Writes one register of every MAX7219 in the chain, at once.
     *
     * \param[in] register_address The address of the register, one of the
     *                             `MAX7219_REG_..._ADDRESS`.
     * \param[in] register_value The value for every module.
     */
    void RegisterWrite(byte register_address, byte register_value) { panel_.GetWired().GetChain().WriteRegisterAll(register_address, register_value); }

    /*! \copydoc MaxMatrixPanel::ShiftLeft(bool, bool) */
    void ShiftLeft(bool rotate = false, bool fill_with_zero = true) { panel_.ShiftLeft(rotate, fill_with_zero); UpdateIfAuto(); }

    /*! \copydoc MaxMatrixPanel::ShiftRight(bool, bool) */
    void ShiftRight(bool rotate = false, bool fill_with_zero = true) { panel_.ShiftRight(rotate, fill_with_zero); UpdateIfAuto(); }

    /*! \copydoc MaxMatrixPanel::ShiftUp(bool) */
    void ShiftUp(bool rotate = false) { panel_.ShiftUp(rotate); UpdateIfAuto(); }

    /*! \copydoc MaxMatrixPanel::ShiftDown(bool) */
    void ShiftDown(bool rotate = false) { panel_.ShiftDown(rotate); UpdateIfAuto(); }
};

/*!
 * \brief Holds the sending back for as long as it exists, as `std::lock_guard` holds a mutex.
 *
 * The constructor turns auto update off, the destructor puts back what was there before. So the
 * picture is sent at the end of the block, also on a `return` or `break` out of it, and of nested
 * ones only the outermost sends.
 *
 * \code{.cpp}
 * {
 *     MaxMatrixBatch batch(matrix);
 *     matrix.Clear();
 *     matrix.SetText("Hi");
 * }  // sent here, at once
 * \endcode
 *
 * \warning It has to be a named variable: an unnamed `MaxMatrixBatch{matrix};` is destroyed at once
 *          and holds nothing back.
 *
 * A picture built over several calls of `loop()` does not fit into a block; that is what
 * MaxMatrix::SetAutoUpdate() and MaxMatrix::Update() are for.
 */
class MaxMatrixBatch
{
  private:
    MaxMatrix& matrix_;
    bool previous_auto_update_;

  public:
    /*!
     * \brief Turns auto update of `matrix` off until the end of the block.
     *
     * \param[in,out] matrix The matrix; it has to outlive the batch.
     */
    explicit MaxMatrixBatch(MaxMatrix& matrix) : matrix_(matrix)
    {
        previous_auto_update_ = matrix_.GetAutoUpdate();
        matrix_.SetAutoUpdate(false);
    }

    /*! \brief Puts auto update back as it was, which sends the picture if it was on. */
    ~MaxMatrixBatch() { matrix_.SetAutoUpdate(previous_auto_update_); }

    /*! \brief Not copyable: a copy would put auto update back twice. */
    MaxMatrixBatch(MaxMatrixBatch const&) = delete;

    /*! \brief Not assignable, as it is not copyable. */
    MaxMatrixBatch& operator=(MaxMatrixBatch const&) = delete;
};

/*!
 * \brief The buffer of a MaxMatrixOf.
 *
 * A base class of it rather than a member, as base classes are made first: the buffer then exists
 * when the MaxMatrix it is handed to empties it.
 *
 * \tparam NumberOfModules The modules of the chain.
 */
template<byte NumberOfModules>
class MaxMatrixBuffer
{
  protected:
    /*! \brief One byte per column. */
    byte buffer_[MAXMATRIX_BUFFER_SIZE(NumberOfModules)];
};

/*!
 * \brief A MaxMatrix with the buffer for a chain of `NumberOfModules` modules.
 *
 * What a sketch makes: the length of the chain is part of the type, the buffer takes eight bytes
 * per module, and a length the library cannot drive does not compile.
 *
 * \code{.cpp}
 * MaxMatrixOf<4> matrix(kDataInPin, kChipSelectPin, kClockPin);
 * \endcode
 *
 * \tparam NumberOfModules The modules of the chain, 1 to `MAXMATRIX_MAX_NUMBER_OF_MODULES`.
 */
template<byte NumberOfModules>
class MaxMatrixOf : private MaxMatrixBuffer<NumberOfModules>, public MaxMatrix
{
    static_assert(NumberOfModules >= 1 && NumberOfModules <= MAXMATRIX_MAX_NUMBER_OF_MODULES,
                  "a chain has 1 to MAXMATRIX_MAX_NUMBER_OF_MODULES modules");

  public:
    /*!
     * \brief Makes the matrix on the chain at these pins.
     *
     * Nothing is sent until Init().
     *
     * \param[in] data_in_pin The pin DIN of the chain is connected to.
     * \param[in] chip_select_pin The pin CS of the chain is connected to.
     * \param[in] clock_pin The pin CLK of the chain is connected to.
     */
    MaxMatrixOf(byte data_in_pin, byte chip_select_pin, byte clock_pin)
        : MaxMatrixBuffer<NumberOfModules>(), MaxMatrix(data_in_pin, chip_select_pin, clock_pin, this->buffer_, NumberOfModules)
    {
    }
};

#endif
