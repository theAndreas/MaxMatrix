// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The header a sketch includes: the matrix it draws on.
 *
 * MaxMatrix drives a chain of 8x8 LED modules with the MAX7219. The class
 * Matrix is the facade over the parts in the folders beside this file, one
 * per subject: `Max7219/` the transfer to the chain, `Matrix/` the buffer as
 * wired and the coordinates as the viewer sees them, `Font/` the sprites of the
 * chars, `Text/` the text that stands or shifts, and `Command/` the lines a
 * control page sends over `Serial`, built on this class. A function here that
 * only passes a call on takes its text from the function it calls.
 */

#ifndef MAXMATRIX_SRC_MAXMATRIX_H_
#define MAXMATRIX_SRC_MAXMATRIX_H_

#include "Arduino.h"
#include "MaxMatrixAttributes.h"
#include "Text/Text.h"
#include "Utility/reference_wrapper.h"

// What a sketch names, it has through this header alone.
#include "Command/Command.h"         // IWYU pragma: export
#include "Font/Font.h"               // IWYU pragma: export
#include "Matrix/Panel.h"            // IWYU pragma: export
#include "Max7219/Chain.h"           // IWYU pragma: export
#include "MaxMatrixBatch.h"          // IWYU pragma: export
#include "MaxMatrixConfig.h"         // IWYU pragma: export
#include "MaxMatrixError.h"          // IWYU pragma: export
#include "MaxMatrixStandardTypes.h"  // IWYU pragma: export
#include "Utility/Byte.h"            // IWYU pragma: export
#include "Utility/array.h"           // IWYU pragma: export
#include "Utility/span.h"            // IWYU pragma: export

namespace max_matrix {

/*! \brief What a Matrix is doing, as Matrix::GetState() tells it. */
enum class State : uint8_t {
  kNone, /*!< Constructed; no Matrix::Init() has succeeded yet. */
  kInit, /*!< Matrix::Init() is running. */
  /*! Nothing to shift: a text or a char shifted in is complete, or stopped. */
  kReady,
  /*! A text of Matrix::SetTextWithShift() is being shifted in. */
  kStringShift,
  /*! A char of Matrix::SetCharWithShift() is being shifted in. */
  kCharShift
};

/*!
 * \brief The matrix a sketch draws on: a chain of 8x8 modules, in the
 *        coordinates the viewer sees.
 *
 * Every function that draws changes the buffer and, with auto update on, the
 * default, sends what changed before it returns: one frame per changed column
 * of a module, eight at most, however much the call drew. A sketch that builds
 * one picture out of many calls holds the sending back with a Batch,
 * or with SetAutoUpdate() and Update(), so that no half of a picture is ever
 * shown. The intensity and the other registers are sent at once either way:
 * they are not part of the picture.
 *
 * A sketch makes it as a MatrixOf, which brings the buffer for its chain:
 *
 * \code{.cpp}
 * max_matrix::MatrixOf<4> matrix{max_matrix::DataInPin{11},
 *                                max_matrix::ChipSelectPin{10},
 *                                max_matrix::ClockPin{13}};
 *
 * void setup() {
 *     matrix.Init();
 *     matrix.SetText("Hi!");
 * }
 * \endcode
 *
 * A shifting text keeps its own pace: ShiftTask() may be called on every
 * `loop()` and takes a step only when the shift interval has passed since the
 * last one, so that no sketch has to time it.
 *
 * A Matrix takes 43 bytes of RAM on the Uno with bit banging and 39 with SPI,
 * besides its buffer of eight bytes per module: a MatrixOf<4> takes 75 and 71.
 *
 * \note With `MAXMATRIX_DEV_ERROR_DETECT` at `STD_ON`, every function here that
 *       fails reports its name and the error to ReportError().
 */
class Matrix {
 public:
  /*!
   * \brief Makes a matrix on the chain at these pins, in the buffer given, and
   *        empties it.
   *
   * Nothing is sent until an Init() succeeds: what is drawn before only
   * changes the buffer, which Init() clears. MatrixOf brings the buffer itself
   * and checks the number of modules when the sketch is compiled; this is for
   * a chain whose length is known only at run time, or a buffer that lies
   * elsewhere. The length of the buffer says how many modules the chain has.
   *
   * \code{.cpp}
   * max_matrix::array<max_matrix::Byte, max_matrix::BufferSize(4)> buffer;
   * max_matrix::Matrix matrix{max_matrix::DataInPin{11},
   *                           max_matrix::ChipSelectPin{10},
   *                           max_matrix::ClockPin{13}, buffer};
   * \endcode
   *
   * \param[in] data_in_pin The pin DIN of the chain is connected to.
   * \param[in] chip_select_pin The pin CS of the chain is connected to.
   * \param[in] clock_pin The pin CLK of the chain is connected to.
   * \param[out] buffer The buffer, `BufferSize()` of the modules in the chain,
   *                    1 to `kMaxNumberOfModules`; Init() refuses any other
   *                    length.
   * \pre `buffer` lives as long as the matrix; otherwise the matrix writes
   *      memory that is no longer the buffer, which is undefined.
   */
  Matrix(DataInPin data_in_pin, ChipSelectPin chip_select_pin,
         ClockPin clock_pin, span<Byte> buffer);

  /*! \brief Leaves the LEDs as they are. */
  ~Matrix() = default;

  /*! \brief No matrix without its pins and its buffer. */
  Matrix() = delete;

  /*! \brief Not copyable: the copy would draw into the same buffer. */
  Matrix(Matrix const&) = delete;

  /*! \brief Not assignable, as it is not copyable. */
  Matrix& operator=(Matrix const&) = delete;

  /*! \brief Not movable: a Batch or a Command may hold on to the matrix. */
  Matrix(Matrix&&) = delete;

  /*! \brief Not movable, as it is not copyable. */
  Matrix& operator=(Matrix&&) = delete;

  /*! \brief Returns the modules of the chain. */
  MAXMATRIX_NODISCARD uint8_t GetNumberOfModules() const {
    return panel_.GetNumberOfModules();
  }

  /*! \brief Returns the columns of the matrix, eight per module. */
  MAXMATRIX_NODISCARD uint8_t GetNumberOfColumns() const {
    return panel_.GetNumberOfColumns();
  }

  /*! \brief Returns what the matrix is doing; State::kReady once a
   * shifted text is complete. */
  MAXMATRIX_NODISCARD State GetState() const { return state_; }

  /*! \brief Returns whether every function that draws sends its result at once;
   * see SetAutoUpdate(). */
  MAXMATRIX_NODISCARD bool GetAutoUpdate() const { return auto_update_; }

  /*! \brief Returns the milliseconds per column of a shifting text; see
   * SetShiftInterval(). */
  MAXMATRIX_NODISCARD uint16_t GetShiftInterval() const {
    return shift_interval_;
  }

  /*! \brief Returns the pin DIN of the chain is connected to. */
  MAXMATRIX_NODISCARD uint8_t GetDataInPin() const {
    return panel_.GetWired().GetChain().GetDataInPin();
  }

  /*! \brief Returns the pin CS of the chain is connected to. */
  MAXMATRIX_NODISCARD uint8_t GetChipSelectPin() const {
    return panel_.GetWired().GetChain().GetChipSelectPin();
  }

  /*! \brief Returns the pin CLK of the chain is connected to. */
  MAXMATRIX_NODISCARD uint8_t GetClockPin() const {
    return panel_.GetWired().GetChain().GetClockPin();
  }

  /*! \brief Returns how the LEDs of every module are wired; see
   * SetModuleType(). */
  MAXMATRIX_NODISCARD ModuleType GetModuleType() const {
    return panel_.GetModuleType();
  }

  /*! \brief Returns how the whole matrix is turned; see SetMatrixOrientation().
   */
  MAXMATRIX_NODISCARD MatrixOrientation GetMatrixOrientation() const {
    return panel_.GetMatrixOrientation();
  }

  /*! \brief Returns the empty columns between two chars of a text. */
  MAXMATRIX_NODISCARD uint8_t GetSpaceBetweenChars() const {
    return text_.GetSpaceBetweenChars();
  }

  /*! \copydoc internal::Panel::GetColumn(X) const */
  MAXMATRIX_NODISCARD Result<Byte> GetColumn(X x) const {
    return MAXMATRIX_REPORT_ERROR("GetColumn", panel_.GetColumn(x));
  }

  /*! \copydoc internal::Panel::GetColumn(Module, uint8_t) const */
  MAXMATRIX_NODISCARD Result<Byte> GetColumn(Module module,
                                             uint8_t column) const {
    return MAXMATRIX_REPORT_ERROR("GetColumn",
                                  panel_.GetColumn(module, column));
  }

  /*!
   * \brief Gives a row of the matrix, one byte per module.
   *
   * \code{.cpp}
   * max_matrix::array<max_matrix::Byte, 4> row;
   * matrix.GetRow(max_matrix::Y{0}, row);
   * \endcode
   *
   * \param[in] y The row, from 0 at the top.
   * \param[out] values The row, bit 7 of each byte the leftmost column of its
   *                    module; left as it was on an error.
   * \return Error::kOk, or Error::kOutOfRange if the row lies off the panel or
   *         `values` has fewer bytes than there are modules.
   */
  MAXMATRIX_NODISCARD Status GetRow(Y y, span<Byte> values) const {
    return MAXMATRIX_REPORT_ERROR("GetRow", panel_.GetRow(y, values));
  }

  /*! \copydoc internal::Panel::GetRow(Module, uint8_t) const */
  MAXMATRIX_NODISCARD Result<Byte> GetRow(Module module, uint8_t row) const {
    return MAXMATRIX_REPORT_ERROR("GetRow", panel_.GetRow(module, row));
  }

  /*! \copydoc internal::Panel::GetDot(Point) const */
  MAXMATRIX_NODISCARD Result<bool> GetDot(Point dot) const {
    return MAXMATRIX_REPORT_ERROR("GetDot", panel_.GetDot(dot));
  }

  /*! \copydoc internal::Text::GetFont() */
  MAXMATRIX_NODISCARD span<uint8_t const> GetFont() const {
    return text_.GetFont();
  }

  /*!
   * \brief Sets the pin DIN of the chain is connected to.
   *
   * \param[in] data_in_pin The pin; Init() makes it an output.
   */
  void SetDataInPin(uint8_t data_in_pin) {
    panel_.GetWired().GetChain().SetDataInPin(data_in_pin);
  }

  /*!
   * \brief Sets the pin CS of the chain is connected to.
   *
   * \param[in] chip_select_pin The pin; Init() makes it an output.
   */
  void SetChipSelectPin(uint8_t chip_select_pin) {
    panel_.GetWired().GetChain().SetChipSelectPin(chip_select_pin);
  }

  /*!
   * \brief Sets the pin CLK of the chain is connected to.
   *
   * \param[in] clock_pin The pin; Init() makes it an output.
   */
  void SetClockPin(uint8_t clock_pin) {
    panel_.GetWired().GetChain().SetClockPin(clock_pin);
  }

  /*!
   * \brief Sets whether what is drawn is sent at once.
   *
   * On, the default, every function that draws sends its result before it
   * returns. Off, only Update() sends. Turning it on sends what was drawn while
   * it was off. A Batch turns it off for a block and back on at its
   * end.
   *
   * \param[in] auto_update True to send after every function that draws.
   */
  void SetAutoUpdate(bool auto_update);

  /*!
   * \brief Sets the milliseconds per column of a shifting text.
   *
   * \param[in] shift_interval The milliseconds; 0 makes every ShiftTask() a
   *                           step.
   */
  void SetShiftInterval(uint16_t shift_interval) {
    shift_interval_ = shift_interval;
  }

  /*!
   * \brief Sets how bright every LED is lit.
   *
   * Sent at once, whatever auto update is.
   *
   * \param[in] intensity The intensity, 0 to 15.
   * \return Error::kOk; Error::kOutOfRange above 15; or Error::kNotSetUp before
   *         an Init() has succeeded. The intensity stays as it was on an error.
   */
  Status SetIntensity(uint8_t intensity);

  /*!
   * \brief Turns the LEDs of every MAX7219 off, or on again.
   *
   * In shutdown the MAX7219s draw next to no current, keep their registers and
   * still take what is drawn: the LEDs show the picture as it is then once the
   * shutdown ends, at the intensity they had. Sent at once, whatever auto
   * update is.
   *
   * \param[in] shutdown True to turn the LEDs off, false to turn them on again.
   * \return Error::kOk, or Error::kNotSetUp before an Init() has succeeded;
   *         nothing changes then.
   */
  Status SetShutdown(bool shutdown);

  /*!
   * \brief Lights every LED at full brightness, or shows the picture again.
   *
   * The display test of the MAX7219 overrides the picture and the intensity
   * without changing either, and also lights a module in shutdown: a check that
   * every LED and every module of the chain works. Sent at once, whatever auto
   * update is.
   *
   * \param[in] display_test True to light every LED, false to show the picture
   *                         again.
   * \return Error::kOk, or Error::kNotSetUp before an Init() has succeeded;
   *         nothing changes then.
   */
  Status SetDisplayTest(bool display_test);

  /*! \copydoc internal::Panel::SetColumn(X, Byte) */
  void SetColumn(X x, Byte value);

  /*! \copydoc internal::Panel::SetColumn(Module, uint8_t, Byte) */
  void SetColumn(Module module, uint8_t column, Byte value);

  /*!
   * \brief Sets a row of the matrix, one byte per module.
   *
   * A row off the panel is left out, as for SetDot(), and so are the modules
   * beyond the bytes given, or the bytes beyond the modules.
   *
   * \param[in] y The row, from 0 at the top.
   * \param[in] values The row, bit 7 of each byte the leftmost column of its
   *                   module.
   */
  void SetRow(Y y, span<Byte const> values);

  /*! \copydoc internal::Panel::SetRow(Module, uint8_t, Byte) */
  void SetRow(Module module, uint8_t row, Byte value);

  /*! \copydoc internal::Panel::SetDot(Point, bool) */
  void SetDot(Point dot, bool value);

  // F-2: in the header, as avr-gcc 7.3 inlines it only from there; defined in
  // the .cpp, it made Shapes 46 bytes of flash larger.
  /*! \copydoc internal::Panel::DrawLine(Point, Point, bool) */
  void DrawLine(Point from, Point to, bool value) {
    panel_.DrawLine(from, to, value);
    UpdateIfAuto();
  }

  /*! \copydoc internal::Panel::DrawRect(Point, Point, bool) */
  void DrawRect(Point corner, Point opposite_corner, bool value);

  /*! \copydoc internal::Panel::FillRect(Point, Point, bool) */
  void FillRect(Point corner, Point opposite_corner, bool value);

  /*! \copydoc internal::Panel::DrawCircle(Point, int16_t, bool) */
  void DrawCircle(Point centre, int16_t radius, bool value);

  /*!
   * \brief Draws a picture from flash, 8 rows high, with its top left corner at
   *        a dot.
   *
   * The picture is a column per byte, bit 0 the top row, as a glyph of a font:
   * a 0 bit turns its dot off. What lies off the matrix is left out.
   *
   * \code{.cpp}
   * constexpr auto kHeart PROGMEM{
   *     max_matrix::make_array<uint8_t>(0x0C, 0x1E, 0x3C, 0x1E, 0x0C)};
   * matrix.DrawBitmap(max_matrix::Point{max_matrix::X{3}, max_matrix::Y{0}},
   *                   kHeart);
   * \endcode
   *
   * \param[in] top_left The top left corner; it may lie off the matrix.
   * \param[in] bitmap The columns, in `PROGMEM`; as many as it holds.
   */
  void DrawBitmap(Point top_left, span<uint8_t const> bitmap) {
    DrawBitmap(top_left, kRowNumberOfModule, bitmap);
  }

  /*! \copydoc internal::Panel::DrawBitmap(Point, uint8_t,
   * span<uint8_t const>) */
  void DrawBitmap(Point top_left, uint8_t height, span<uint8_t const> bitmap);

  /*!
   * \brief Draws a char with its top left corner at a dot.
   *
   * What lies off the matrix is left out.
   *
   * \param[in] top_left The top left corner; it may lie off the matrix.
   * \param[in] c The char, latin-1.
   * \return Error::kOk, or Error::kUnknownChar if the font has no sprite for
   *         the char; nothing is drawn then.
   */
  Status SetChar(Point top_left, char c) {
    return UpdateIfAuto(
        MAXMATRIX_REPORT_ERROR("SetChar", text_.SetChar(panel_, top_left, c)));
  }

  /*!
   * \brief Starts shifting a char in from the right, until it and the space
   *        behind it have come in.
   *
   * ShiftTask() takes the steps, one column each; GetState() is
   * State::kCharShift until the char stands, the space behind it at the
   * right edge, so that a char shifted in after it keeps its distance.
   *
   * \param[in] c The char, latin-1.
   * \return Error::kOk; Error::kNotSetUp before an Init() has succeeded; or
   *         Error::kUnknownChar if the font has no sprite for the char. Nothing
   *         starts on an error.
   */
  Status SetCharWithShift(char c);

  /*!
   * \brief Draws a text from the left edge, as far as it fits.
   *
   * A text in quotes or a char array passes as it is; a text known only by a
   * pointer is passed with its length, `{text, strlen(text)}`.
   *
   * \param[in] text The text, UTF-8 or latin-1, up to its terminating null
   *                 character or the end of the span, whichever comes first.
   * \return Error::kOk; Error::kNoText if `text` is an empty span; or
   *         Error::kUnknownChar if a char of the text is not in the font, with
   *         the chars before it drawn.
   */
  Status SetText(span<char const> text) {
    return UpdateIfAuto(
        MAXMATRIX_REPORT_ERROR("SetText", text_.SetText(panel_, text)));
  }

  /*!
   * \brief Starts shifting a text in from the right, until its last char and
   *        the space behind it have come in, or over and over.
   *
   * ShiftTask() takes the steps, one column each; GetState() is
   * State::kStringShift until the text stands. A text that repeats goes on with
   * its first char right after its last, so a few spaces at its end keep the
   * two apart; it shifts until StopShift() or the next shift starts.
   *
   * \param[in] text The text, UTF-8 or latin-1, up to its terminating null
   *                 character or the end of the span, as for SetText().
   * \param[in] repeat True to start over after the last char, without end.
   * \return Error::kOk; Error::kNotSetUp before an Init() has succeeded; or
   *         Error::kNoText if `text` is an empty span. Nothing starts on an
   *         error.
   * \pre `text` lives until the text stands, or as long as it repeats;
   *      otherwise ShiftTask() reads memory that is no longer the text, which
   *      is undefined.
   */
  Status SetTextWithShift(span<char const> text, bool repeat = false);

  /*!
   * \brief Stops a shifting text or char where it is.
   *
   * The picture stays as it is, and GetState() is State::kReady;
   * ShiftTask() takes no step until the next shift starts. Without a shift, it
   * does nothing.
   */
  void StopShift();

  /*!
   * \brief Sets the empty columns between two chars of a text.
   *
   * \param[in] space_between_chars The number of empty columns.
   */
  void SetSpaceBetweenChars(uint8_t space_between_chars) {
    text_.SetSpaceBetweenChars(space_between_chars);
  }

  /*!
   * \brief Sets the font every char is drawn in from now on.
   *
   * A font of MD_MAX72XX serves with its bytes as they are, in make_array()
   * rather than its C array; Font describes the format. Chars drawn already
   * stay as they are, and a char that shifts in goes on in the font it started
   * in. kDefaultFont is the font MaxMatrix starts with.
   *
   * \code{.cpp}
   * constexpr auto kBigFont PROGMEM{
   *     max_matrix::make_array<uint8_t>('F', 1, ...)};
   *
   * matrix.SetFont(kBigFont);
   * \endcode
   *
   * The length of the font is that of its array, which the span takes from
   * it, so that the whole font is checked once, here, and no char can be read
   * past its end. A font known only by its address, as one of a table of
   * fonts, is passed with its length, `{font, size}`. A font in RAM, without
   * `PROGMEM`, fails as well, almost always, as the Uno reads it from flash.
   *
   * \param[in] font The font, in flash.
   * \return Error::kOk, or Error::kNotAFont if Font::CheckFont() does not take
   *         it; the font then stays as it was.
   * \pre `font` lives as long as it is set, as a `PROGMEM` array does;
   *      otherwise every char is drawn from memory that is no longer the font,
   *      which is undefined.
   */
  Status SetFont(span<uint8_t const> font) {
    return MAXMATRIX_REPORT_ERROR("SetFont", text_.SetFont(font));
  }

  /*!
   * \brief Draws a sprite with its top left corner at a dot.
   *
   * What lies off the matrix is left out.
   *
   * \param[in] top_left The top left corner; it may lie off the matrix.
   * \param[in] sprite The sprite.
   */
  void SetSprite(Point top_left, Sprite const& sprite);

  /*! \copydoc internal::Panel::SetModuleType(ModuleType) */
  void SetModuleType(ModuleType module_type);

  /*! \copydoc internal::Panel::SetMatrixOrientation(MatrixOrientation)
   */
  void SetMatrixOrientation(MatrixOrientation matrix_orientation);

  /*!
   * \brief Sets the chain up: the pins, every register of every MAX7219, and
   *        every LED off.
   *
   * The intensity is set to its start value. The cleared matrix is sent even
   * with auto update off, as after power-up the LEDs show anything. A shift
   * that runs ends, as the panel it draws on is cleared.
   *
   * \return Error::kOk, or Error::kOutOfRange if a pin is none of the board's,
   *         `NUM_DIGITAL_PINS` or above, or the buffer is no chain of 1 to
   *         `kMaxNumberOfModules` modules; then nothing is sent, a matrix set
   *         up before keeps its state and drives the pins it had, and one never
   *         set up sends nothing until an Init() succeeds.
   */
  Status Init();

  /*!
   * \brief Shifts a text or a char one column further, if the shift interval
   *        has passed.
   *
   * Call it on every `loop()` until GetState() is State::kReady again, or on
   * every `loop()` at all: without a shift, it changes nothing.
   * It takes a step only when the shift interval has passed since the last one,
   * and with an interval of 0 on every call. A call that comes late takes one
   * step, not the ones it missed: catching up would make the text jump after
   * `loop()` was held up.
   */
  void ShiftTask();

  /*!
   * \brief Sends what was drawn since the last update.
   *
   * Only needed with auto update off; see Batch.
   */
  void Update() { panel_.GetWired().Send(); }

  // F-2: in the header, as for DrawLine(); defined in the .cpp, it made
  // SerialControl 24 bytes of flash larger.
  /*! \brief Turns every LED off. */
  void Clear() {
    panel_.GetWired().Clear();
    UpdateIfAuto();
  }

  /*!
   * \brief Writes one register of every MAX7219 in the chain, at once.
   *
   * \param[in] address The register, as `max7219::Register::kIntensity`.
   * \param[in] value The value for every module, of the constants in `max7219`
   *                  or a number.
   */
  void RegisterWrite(max7219::Register address, uint8_t value) {
    panel_.GetWired().GetChain().WriteRegisterAll(address, value,
                                                  panel_.GetNumberOfModules());
  }

  /*! \copydoc internal::Panel::ShiftLeft(ShiftFill) */
  void ShiftLeft(ShiftFill fill = ShiftFill::kZero);

  /*! \copydoc internal::Panel::ShiftLeft(Byte) */
  Byte ShiftLeft(Byte shifted_in);

  /*! \copydoc internal::Panel::ShiftRight(ShiftFill) */
  void ShiftRight(ShiftFill fill = ShiftFill::kZero);

  /*! \copydoc internal::Panel::ShiftRight(Byte) */
  Byte ShiftRight(Byte shifted_in);

  /*! \copydoc internal::Panel::ShiftUp(ShiftFill) */
  void ShiftUp(ShiftFill fill = ShiftFill::kZero);

  /*! \copydoc internal::Panel::ShiftDown(ShiftFill) */
  void ShiftDown(ShiftFill fill = ShiftFill::kZero);

 private:
  // Sends what changed, if auto update is on; every public function that draws
  // ends with it.
  void UpdateIfAuto();
  MAXMATRIX_NODISCARD Status UpdateIfAuto(Status status);
  void StartShift(State shift_state);

  internal::Panel panel_;
  internal::Text text_;
  State state_{State::kNone};
  bool auto_update_{true};
  // E-2: as for the space between chars in Text.
  static_assert(static_cast<uint16_t>(MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE) ==
                    MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE,
                "MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE is 0 to 65535");
  uint16_t shift_interval_{MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE};
  uint32_t last_shift_{0};
};

namespace internal {

/*!
 * \brief The buffer of a MatrixOf.
 *
 * A base class of it rather than a member, as base classes are made first: the
 * buffer then exists when the Matrix it is handed to empties it.
 *
 * \tparam NumberOfModules The modules of the chain.
 */
template <uint8_t NumberOfModules>
class Buffer {
 protected:
  /*! \brief One byte per column. */
  array<Byte, BufferSize(NumberOfModules)> buffer_;
};

}  // namespace internal

// misc-multiple-inheritance, suppressed below: the buffer is a base of its own,
// so that it is made before the Matrix it is handed to (internal::Buffer); it
// is private and has no virtual function, so that nothing but the constructor
// reaches it. C-8, as both bases are reused by deriving: the buffer for that
// order, and Matrix, as a MatrixOf is the matrix a sketch hands wherever one is
// taken, Command::Feed() and Batch among them, without its length.
/*!
 * \brief A Matrix with the buffer for a chain of `NumberOfModules` modules.
 *
 * What a sketch makes: the length of the chain is part of the type, the buffer
 * takes eight bytes per module, and a length the library cannot drive does not
 * compile.
 *
 * \code{.cpp}
 * max_matrix::MatrixOf<4> matrix{max_matrix::DataInPin{11},
 *                                max_matrix::ChipSelectPin{10},
 *                                max_matrix::ClockPin{13}};
 * \endcode
 *
 * \tparam NumberOfModules The modules of the chain, 1 to `kMaxNumberOfModules`.
 */
template <uint8_t NumberOfModules>  // NOLINTNEXTLINE(misc-multiple-inheritance)
class MatrixOf : private internal::Buffer<NumberOfModules>, public Matrix {
  static_assert((NumberOfModules >= 1) &&
                    (NumberOfModules <= kMaxNumberOfModules),
                "a chain has 1 to kMaxNumberOfModules modules");

 public:
  /*!
   * \brief Makes the matrix on the chain at these pins.
   *
   * Nothing is sent until an Init() succeeds: what is drawn before only
   * changes the buffer, which Init() clears.
   *
   * \param[in] data_in_pin The pin DIN of the chain is connected to.
   * \param[in] chip_select_pin The pin CS of the chain is connected to.
   * \param[in] clock_pin The pin CLK of the chain is connected to.
   */
  MatrixOf(DataInPin data_in_pin, ChipSelectPin chip_select_pin,
           ClockPin clock_pin)
      : internal::Buffer<NumberOfModules>{},
        Matrix{data_in_pin, chip_select_pin, clock_pin, this->buffer_} {}
};

}  // namespace max_matrix

#endif  // MAXMATRIX_SRC_MAXMATRIX_H_
