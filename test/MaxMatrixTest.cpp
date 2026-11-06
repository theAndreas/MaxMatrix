/* Host-side tests of MaxMatrix against the Arduino stand-in in mock/.
 *
 * Built twice by CMakeLists.txt, once with bit banging and once with MAXMATRIX_USE_SPI=STD_ON, because the
 * two transfer paths are separate code. What this cannot show is anything about timing or the electrical
 * side - whether a real MAX7219 accepts the frames is only known on hardware. */
#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <tuple>
#include <vector>
#include "Arduino.h"

/* The parts of the library and the matrix buffer are private members of MaxMatrix, and some of what is
 * tested here has no public path: MaxMatrixWired::setColumnOnAllModules is only reached through Clear(),
 * which zeroes the buffer afterwards and so hides a wrong index. Included after GoogleTest and the
 * standard headers, which must not see it - defined above them, it rewrites their classes too. */
#define private public  // NOLINT(readability-identifier-naming): what it redefines is a keyword
#include "MaxMatrix.h"
#undef private

static byte const kDataInPin = 11;
static byte const kChipSelectPin = 10;
static byte const kClockPin = 13;

/* every error by name, in the order of the enum */
static char const* const kErrorNames[] = {
    "MaxMatrixError::kOk", "MaxMatrixError::kOutOfRange", "MaxMatrixError::kUnknownChar", "MaxMatrixError::kNoText", "MaxMatrixError::kNotANumber",
    "MaxMatrixError::kUnknownCommand", "MaxMatrixError::kLineTooLong"
};
static uint8_t const kNumberOfErrors = sizeof(kErrorNames) / sizeof(kErrorNames[0]);

/* GoogleTest would print an error as its number */
static void PrintTo(MaxMatrixError error, std::ostream* out)
{
    if(static_cast<uint8_t>(error) < kNumberOfErrors) *out << kErrorNames[static_cast<uint8_t>(error)];
    else *out << "error " << static_cast<uint16_t>(error);
}

/* what Feed() returns, compared with a state as with an error: a line is done only if it did not fail as well */
static bool operator==(MaxMatrixResult<MaxMatrixCommandState> const& result, MaxMatrixCommandState state)
{
    return result.IsOk() && result.GetValue() == state;
}

static void PrintTo(MaxMatrixResult<MaxMatrixCommandState> const& result, std::ostream* out)
{
    if(!result.IsOk()) PrintTo(result.GetError(), out);
    else if(result.GetValue() == MaxMatrixCommandState::kDone) *out << "MaxMatrixCommandState::kDone";
    else if(result.GetValue() == MaxMatrixCommandState::kAnswered) *out << "MaxMatrixCommandState::kAnswered";
    else *out << "MaxMatrixCommandState::kPending";
}

/* where MaxMatrixCommand answers in a test: a Print that keeps what it is given */
class TextPrint : public Print {
  public:
    std::string text;

    size_t write(uint8_t c) override
    {
        text += static_cast<char>(c);
        return 1;
    }
};

/* the text of F() and GetErrorText(), which lies in RAM on the host */
static std::string Text(__FlashStringHelper const* text)
{
    return reinterpret_cast<char const*>(text);
}

/* a whole line to a command of its own, and why it failed */
static MaxMatrixError FeedText(MaxMatrix& matrix, std::string const& text)
{
    MaxMatrixCommand command;
    TextPrint answer;
    MaxMatrixResult<MaxMatrixCommandState> result = MaxMatrixCommandState::kPending;

    for(char c : text) result = command.Feed(matrix, c, answer);
    return result.GetError();
}

/* What the library reported, with MAXMATRIX_DEV_ERROR_DETECT on: the definition here wins over the weak one of the
 * library, which prints. With it off the list stays empty, so that a test can expect a report in every variant. */
typedef std::pair<std::string, MaxMatrixError> Report;
static bool const kDevErrorDetect = (MAXMATRIX_DEV_ERROR_DETECT == STD_ON);
static std::vector<Report> reports;

#if(MAXMATRIX_DEV_ERROR_DETECT == STD_ON)
void MaxMatrixReportError(__FlashStringHelper const* api, MaxMatrixError error)
{
    reports.push_back(Report(Text(api), error));
}
#endif

/* GoogleTest prints a vector element by element, where a failed memcmp only says that something differs */
static std::vector<byte> Bytes(byte const* data, size_t size)
{
    return std::vector<byte>(data, data + size);
}

/* the library sends the pair of module 0 first, so it ends up in the chip at the far end of the chain */
static Max7219 const& ChipOfModule(byte module)
{
    return mock::chain.chips[MAXMATRIX_NUMBER_OF_MODULES - 1 - module];
}

/* what the chain shows, laid out like matrix_buffer_: a column of a module is a digit, a row a segment */
static std::vector<byte> Display()
{
    std::vector<byte> columns;
    for(byte module = 0; module < MAXMATRIX_NUMBER_OF_MODULES; module++) {
        for(byte column = 0; column < MAXMATRIX_COLUMN_NUMBER_OF_MODULE; column++) {
            columns.push_back(ChipOfModule(module).Segments(column));
        }
    }
    return columns;
}

class MaxMatrixTest : public ::testing::Test {
  protected:
    MaxMatrix matrix_{kDataInPin, kChipSelectPin, kClockPin};

    void SetUp() override
    {
        mock::Reset(kDataInPin, kChipSelectPin, kClockPin, MAXMATRIX_NUMBER_OF_MODULES);
        reports.clear();
        matrix_.Init();
        /* the tests step a shifting text with every call; the timing of the steps has tests of its own */
        matrix_.SetShiftInterval(0);
    }

    /* whatever a test did, the LEDs have to show what the buffer says - the get functions read the buffer, because
     * the MAX7219 cannot be read back, so a buffer that drifts from the display makes them lie; and every bit went out
     * within a frame, with the interrupts off, which are on again afterwards */
    void TearDown() override
    {
        EXPECT_EQ(Display(), Bytes(matrix_.panel_.wired_.matrix_buffer_, sizeof(matrix_.panel_.wired_.matrix_buffer_))) << "display differs from matrix_buffer_";
        EXPECT_EQ(mock::errors, 0) << "a transfer outside a frame, or a port written with the interrupts on";
        EXPECT_EQ(SREG, 0x80) << "the interrupts left off";
    }

    /* the ShiftTask() calls the last ShiftWholeText() took, each one a step of the text on the panel */
    uint16_t shift_calls_ = 0;

    /* returns the number of frames the shift sent to the chain */
    int32_t ShiftWholeText(char* text)
    {
        matrix_.Clear();
        int32_t frames_before = mock::frames;
        matrix_.SetTextWithShift(text);
        for(shift_calls_ = 0; shift_calls_ < 1000 && matrix_.GetState() != MaxMatrixState::kReady; shift_calls_++) matrix_.ShiftTask();
        return mock::frames - frames_before;
    }
};

static MaxMatrixMatrixOrientation const kMatrixOrientations[] = {
    MaxMatrixMatrixOrientation::k0, MaxMatrixMatrixOrientation::k180
};

static MaxMatrixModuleOrientation const kModuleOrientations[] = {
    MaxMatrixModuleOrientation::k0, MaxMatrixModuleOrientation::k90,
    MaxMatrixModuleOrientation::k180, MaxMatrixModuleOrientation::k270
};

typedef std::tuple<MaxMatrixMatrixOrientation, MaxMatrixModuleOrientation> OrientationPair;

/* every test of this fixture runs in all eight combinations of matrix and module orientation */
class MaxMatrixOrientationTest : public MaxMatrixTest, public ::testing::WithParamInterface<OrientationPair> {
  protected:
    void SetUp() override
    {
        MaxMatrixTest::SetUp();
        matrix_.Clear();
        matrix_.SetMatrixOrientation(std::get<0>(GetParam()));
        matrix_.SetModuleOrientation(std::get<1>(GetParam()));
    }
};

static std::string OrientationName(::testing::TestParamInfo<OrientationPair> const& info)
{
    static char const* const kNames[] = { "0", "90", "180", "270" };
    return std::string("matrix") + (std::get<0>(info.param) == MaxMatrixMatrixOrientation::k180 ? "180" : "0") +
           "_module" + kNames[static_cast<uint8_t>(std::get<1>(info.param))];
}

INSTANTIATE_TEST_SUITE_P(AllOrientations, MaxMatrixOrientationTest,
    ::testing::Combine(::testing::ValuesIn(kMatrixOrientations), ::testing::ValuesIn(kModuleOrientations)),
    OrientationName);

/* the bit position used to run on across modules, so only module 0 got its row */
TEST_P(MaxMatrixOrientationTest, RowRoundTrip)
{
    MaxMatrixRow in, out;
    for(byte module = 0; module < MAXMATRIX_NUMBER_OF_MODULES; module++) in[module] = 0xA5 ^ (module * 0x3C);

    for(byte row = 0; row < MAXMATRIX_ROW_NUMBER_OF_MODULE; row++) {
        SCOPED_TRACE(::testing::Message() << "Row " << static_cast<uint16_t>(row));
        matrix_.SetRow(row, in);
        EXPECT_EQ(matrix_.GetRow(row, out).GetError(), MaxMatrixError::kOk);
        EXPECT_EQ(Bytes(out, sizeof(out)), Bytes(in, sizeof(in)));
    }
}

/* the display has to follow the buffer during the shift too, not only once the text has left it */
TEST_P(MaxMatrixOrientationTest, DisplayFollowsTheShift)
{
    char text[] = "Hi \xC4!";

    matrix_.SetTextWithShift(text);
    for(uint16_t step = 0; step < 1000 && matrix_.GetState() != MaxMatrixState::kReady; step++) {
        matrix_.ShiftTask();
        ASSERT_EQ(Display(), Bytes(matrix_.panel_.wired_.matrix_buffer_, sizeof(matrix_.panel_.wired_.matrix_buffer_))) << "after step " << step;
    }
}

/* Whatever a call draws, it sends one frame per changed column of a module at most. Every dot and column used to be a
 * frame of its own: SetText("Hi!") in module orientation 270 sent 64, and each step of a shifting text nine. */
TEST_P(MaxMatrixOrientationTest, NoCallSendsMoreThanEightFrames)
{
    struct {
        char const* name;
        void (*draw)(MaxMatrix&);
    } const calls[] = {
        { "SetText", [](MaxMatrix& m) { m.SetText("Hi!"); } },
        { "SetChar off the top row", [](MaxMatrix& m) { m.SetChar(9, 1, 'A'); } },
        { "SetRow", [](MaxMatrix& m) { MaxMatrixRow row = { 0xFF, 0x0F, 0xF0, 0x3C }; m.SetRow(5, row); } },
        { "SetColumn", [](MaxMatrix& m) { m.SetColumn(17, 0x5A); } },
        { "ShiftUp", [](MaxMatrix& m) { m.ShiftUp(true); } },
        { "clear", [](MaxMatrix& m) { m.Clear(); } },
    };
    char text[] = "Hi \xC4!";

    for(auto const& call : calls) {
        int32_t frames_before = mock::frames;
        call.draw(matrix_);
        EXPECT_LE(mock::frames - frames_before, MAXMATRIX_COLUMN_NUMBER_OF_MODULE) << call.name;
    }
    matrix_.SetTextWithShift(text);
    for(uint16_t step = 0; step < 1000 && matrix_.GetState() != MaxMatrixState::kReady; step++) {
        int32_t frames_before = mock::frames;
        matrix_.ShiftTask();
        ASSERT_LE(mock::frames - frames_before, MAXMATRIX_COLUMN_NUMBER_OF_MODULE) << "shift step " << step;
    }
}

static bool Pattern(int16_t x, int16_t y)
{
    return (x * x + 3 * y + x * y) % 3 == 0;
}

/* a picture as rows of '#' and '.', so that a failure prints both pictures */
template<typename DotType>
static std::vector<std::string> Picture(DotType dot)
{
    std::vector<std::string> rows;
    for(int16_t y = 0; y < MAXMATRIX_ROW_NUMBER_OF_MODULE; y++) {
        std::string row;
        for(int16_t x = 0; x < MAXMATRIX_NUMBER_OF_COLUMNS; x++) row += dot(x, y) ? '#' : '.';
        rows.push_back(row);
    }
    return rows;
}

/* the picture as the library reports it, through GetDot */
static std::vector<std::string> ShownPicture(MaxMatrix& matrix)
{
    return Picture([&matrix](int16_t x, int16_t y) { return matrix.GetDot(x, y).GetValue(); });
}

static void DrawPattern(MaxMatrix& matrix)
{
    for(byte x = 0; x < MAXMATRIX_NUMBER_OF_COLUMNS; x++) {
        for(byte y = 0; y < MAXMATRIX_ROW_NUMBER_OF_MODULE; y++) matrix.SetDot(x, y, Pattern(x, y));
    }
}

/* rows and columns are two views of one picture: in orientation 180 SetColumn turned the whole panel while SetRow
 * turned each module */
TEST_P(MaxMatrixOrientationTest, RowsAndColumnsAgree)
{
    MaxMatrixRow row;
    for(byte module = 0; module < MAXMATRIX_NUMBER_OF_MODULES; module++) row[module] = 0x81 ^ (module * 0x16);

    matrix_.SetRow(2, row);
    EXPECT_EQ(ShownPicture(matrix_), Picture([&row](int16_t x, int16_t y) {
        return y == 2 && bitRead(row[x / MAXMATRIX_COLUMN_NUMBER_OF_MODULE], 7 - x % MAXMATRIX_COLUMN_NUMBER_OF_MODULE);
    }));

    matrix_.Clear();
    matrix_.SetColumn(5, 0xA3);
    EXPECT_EQ(ShownPicture(matrix_), Picture([](int16_t x, int16_t y) { return x == 5 && bitRead(0xA3, y); }));
    for(byte y = 0; y < MAXMATRIX_ROW_NUMBER_OF_MODULE; y++) {
        MaxMatrixResult<byte> value = matrix_.GetRow(0, y);
        EXPECT_EQ(value.GetError(), MaxMatrixError::kOk);
        EXPECT_EQ(value.GetValue(), bitRead(0xA3, y) ? 1 << (7 - 5) : 0) << "Row " << static_cast<uint16_t>(y);
    }
}

/* every shift moves the whole picture by one dot as the viewer sees it, across the modules */
TEST_P(MaxMatrixOrientationTest, ShiftsMoveThePicture)
{
    struct {
        char const* name;
        void (*shift)(MaxMatrix&);
        int16_t source_x, source_y;
        bool rotate;
    } const cases[] = {
        { "ShiftLeft", [](MaxMatrix& m) { m.ShiftLeft(false, true); }, 1, 0, false },
        { "ShiftLeft rotating", [](MaxMatrix& m) { m.ShiftLeft(true, false); }, 1, 0, true },
        { "ShiftRight", [](MaxMatrix& m) { m.ShiftRight(false, true); }, -1, 0, false },
        { "ShiftRight rotating", [](MaxMatrix& m) { m.ShiftRight(true, false); }, -1, 0, true },
        { "ShiftUp", [](MaxMatrix& m) { m.ShiftUp(false); }, 0, 1, false },
        { "ShiftUp rotating", [](MaxMatrix& m) { m.ShiftUp(true); }, 0, 1, true },
        { "ShiftDown", [](MaxMatrix& m) { m.ShiftDown(false); }, 0, -1, false },
        { "ShiftDown rotating", [](MaxMatrix& m) { m.ShiftDown(true); }, 0, -1, true },
    };
    int16_t const width = MAXMATRIX_NUMBER_OF_COLUMNS, height = MAXMATRIX_ROW_NUMBER_OF_MODULE;

    for(auto const& test_case : cases) {
        matrix_.Clear();
        DrawPattern(matrix_);
        test_case.shift(matrix_);
        EXPECT_EQ(ShownPicture(matrix_), Picture([&test_case](int16_t x, int16_t y) {
            int16_t source_x = x + test_case.source_x, source_y = y + test_case.source_y;
            if(test_case.rotate) return Pattern((source_x + width) % width, (source_y + height) % height);
            return source_x >= 0 && source_x < width && source_y >= 0 && source_y < height && Pattern(source_x, source_y);
        })) << test_case.name;
    }
}

/* changing an orientation turns the buffer along, so the viewer still sees the same picture; changing the module
 * orientation to or from 180 turned each module while SetColumn had turned the whole panel */
TEST_P(MaxMatrixOrientationTest, PictureStaysWhenTheOrientationChanges)
{
    for(MaxMatrixMatrixOrientation matrix_orientation : kMatrixOrientations) {
        for(MaxMatrixModuleOrientation module_orientation : kModuleOrientations) {
            matrix_.SetMatrixOrientation(std::get<0>(GetParam()));
            matrix_.SetModuleOrientation(std::get<1>(GetParam()));
            matrix_.Clear();
            DrawPattern(matrix_);
            matrix_.SetMatrixOrientation(matrix_orientation);
            matrix_.SetModuleOrientation(module_orientation);
            EXPECT_EQ(ShownPicture(matrix_), Picture(Pattern))
                << "to matrix " << static_cast<uint16_t>(matrix_orientation) << ", module " << static_cast<uint16_t>(module_orientation);
        }
    }
}

TEST_P(MaxMatrixOrientationTest, DotRoundTrip)
{
    for(byte column = 0; column < MAXMATRIX_NUMBER_OF_COLUMNS; column++) {
        for(byte row = 0; row < MAXMATRIX_ROW_NUMBER_OF_MODULE; row++) {
            SCOPED_TRACE(::testing::Message() << "Column " << static_cast<uint16_t>(column) << ", Row " << static_cast<uint16_t>(row));
            bool expected = (column * 3 + row) & 1;
            matrix_.SetDot(column, row, expected);
            MaxMatrixResult<bool> value = matrix_.GetDot(column, row);
            EXPECT_EQ(value.GetError(), MaxMatrixError::kOk);
            EXPECT_EQ(value.GetValue(), expected);
        }
    }
}

/* Which orientations each module needs, as the README tells a user: the module orientation follows from the
 * wiring, the matrix orientation from whether the board is mounted upside down. PAROLA and the other three wirings
 * are mirror images, which no orientation gives (OIL item 9). */
struct Module {
    char const* name;
    Max7219Wiring const* wiring;
    bool upside_down;
    MaxMatrixMatrixOrientation matrix_orientation;
    MaxMatrixModuleOrientation module_orientation;
};

static Module const kModules[] = {
    { "DR0CR0RR1", &kMax7219Wirings[1], false, MaxMatrixMatrixOrientation::k0, MaxMatrixModuleOrientation::k0 },
    { "ICSTATION", &kMaX7219WiringIcstation, false, MaxMatrixMatrixOrientation::k0, MaxMatrixModuleOrientation::k90 },
    { "GENERIC", &kMaX7219WiringGeneric, false, MaxMatrixMatrixOrientation::k0, MaxMatrixModuleOrientation::k180 },
    { "FC16", &kMaX7219WiringFC16, false, MaxMatrixMatrixOrientation::k0, MaxMatrixModuleOrientation::k270 },
    { "DR0CR0RR1_upside_down", &kMax7219Wirings[1], true, MaxMatrixMatrixOrientation::k180, MaxMatrixModuleOrientation::k0 },
    { "ICSTATION_upside_down", &kMaX7219WiringIcstation, true, MaxMatrixMatrixOrientation::k180, MaxMatrixModuleOrientation::k90 },
    { "GENERIC_upside_down", &kMaX7219WiringGeneric, true, MaxMatrixMatrixOrientation::k180, MaxMatrixModuleOrientation::k180 },
    { "FC16_upside_down", &kMaX7219WiringFC16, true, MaxMatrixMatrixOrientation::k180, MaxMatrixModuleOrientation::k270 },
};

class MaxMatrixModuleTest : public MaxMatrixTest, public ::testing::WithParamInterface<Module> {};

static std::string ModuleName(::testing::TestParamInfo<Module> const& info)
{
    return info.param.name;
}

INSTANTIATE_TEST_SUITE_P(Modules, MaxMatrixModuleTest, ::testing::ValuesIn(kModules), ModuleName);

/* a dot set at X, Y is lit at X, Y of the panel; the pattern has no symmetry a wrong mapping could hide behind */
TEST_P(MaxMatrixModuleTest, DotsAppearWhereTheyAreSet)
{
    Module const& module = GetParam();
    auto pattern = [](uint16_t x, uint8_t y) { return (x * x + 3 * y + x * y) % 3 == 0; };

    matrix_.SetMatrixOrientation(module.matrix_orientation);
    matrix_.SetModuleOrientation(module.module_orientation);
    for(byte x = 0; x < MAXMATRIX_NUMBER_OF_COLUMNS; x++) {
        for(byte y = 0; y < MAXMATRIX_ROW_NUMBER_OF_MODULE; y++) matrix_.SetDot(x, y, pattern(x, y));
    }
    for(uint16_t x = 0; x < MAXMATRIX_NUMBER_OF_COLUMNS; x++) {
        for(uint8_t y = 0; y < MAXMATRIX_ROW_NUMBER_OF_MODULE; y++) {
            EXPECT_EQ(mock::chain.IsLit(*module.wiring, x, y, module.upside_down), pattern(x, y)) << "X " << x << ", Y " << y;
        }
    }
}

TEST_F(MaxMatrixTest, InitConfiguresEveryChip)
{
    for(byte module = 0; module < MAXMATRIX_NUMBER_OF_MODULES; module++) {
        SCOPED_TRACE(::testing::Message() << "Module " << static_cast<uint16_t>(module));
        Max7219 const& chip = ChipOfModule(module);
        EXPECT_EQ(chip.shutdown, MAX7219_REG_SHUTDOWN_MODE_NORMAL_OPERATION);
        EXPECT_EQ(chip.display_test, MAX7219_REG_DISPLAY_TEST_NORMAL_OPERATION);
        EXPECT_EQ(chip.scan_limit, MAX7219_REG_SCAN_LIMIT_DISPLAY_DIGIT_0_TO_7);
        EXPECT_EQ(chip.decode_mode, MAX7219_REG_DECODE_MODE_NO_DECODE);
        EXPECT_EQ(chip.intensity, MAXMATRIX_INTENSITY_INIT_VALUE);
    }
}

TEST_F(MaxMatrixTest, IntensityReachesEveryChip)
{
    EXPECT_EQ(matrix_.SetIntensity(MAX7219_REG_INTENSITY_MAX_VALUE).GetError(), MaxMatrixError::kOk);
    for(byte module = 0; module < MAXMATRIX_NUMBER_OF_MODULES; module++) {
        EXPECT_EQ(ChipOfModule(module).intensity, MAX7219_REG_INTENSITY_MAX_VALUE) << "Module " << static_cast<uint16_t>(module);
    }
}

TEST_F(MaxMatrixTest, OutOfRangeChangesNothing)
{
    std::vector<byte> before = Bytes(matrix_.panel_.wired_.matrix_buffer_, sizeof(matrix_.panel_.wired_.matrix_buffer_));
    int32_t frames_before = mock::frames;

    EXPECT_EQ(matrix_.SetIntensity(MAX7219_REG_INTENSITY_MAX_VALUE + 1).GetError(), MaxMatrixError::kOutOfRange);
    EXPECT_EQ(Bytes(matrix_.panel_.wired_.matrix_buffer_, sizeof(matrix_.panel_.wired_.matrix_buffer_)), before);
    EXPECT_EQ(mock::frames, frames_before);
}

/* every way a call can fail, with the reason it gives: with stdReturnType a test stayed green when another check
 * than the one meant failed. A failing call changes nothing, draws nothing and starts no shift. "999x" is no number
 * rather than too large, although the digits run past the maximum before the letter comes. With
 * MAXMATRIX_DEV_ERROR_DETECT, the function of MaxMatrix that failed reports it once, under its own name. The command
 * reports nothing of its own, as a wrong line is no mistake of the sketch and its answer says what was wrong; only a
 * line that fails in a function of MaxMatrix, text= with a char the font lacks, is reported by that function. */
TEST_F(MaxMatrixTest, EveryErrorSaysWhy)
{
    struct {
        char const* name;
        MaxMatrixError (*call)(MaxMatrix&);
        MaxMatrixError expected;
        char const* api;
    } const cases[] = {
        { "GetColumn left", [](MaxMatrix& m) { return m.GetColumn(-1).GetError(); }, MaxMatrixError::kOutOfRange, "GetColumn" },
        { "GetColumn right", [](MaxMatrix& m) { return m.GetColumn(MAXMATRIX_NUMBER_OF_COLUMNS).GetError(); }, MaxMatrixError::kOutOfRange, "GetColumn" },
        { "GetColumn no module", [](MaxMatrix& m) { return m.GetColumn(MAXMATRIX_NUMBER_OF_MODULES, 0).GetError(); }, MaxMatrixError::kOutOfRange, "GetColumn" },
        { "GetColumn off its module", [](MaxMatrix& m) { return m.GetColumn(0, MAXMATRIX_COLUMN_NUMBER_OF_MODULE).GetError(); }, MaxMatrixError::kOutOfRange, "GetColumn" },
        { "GetRow above", [](MaxMatrix& m) { MaxMatrixRow row; return m.GetRow(-1, row).GetError(); }, MaxMatrixError::kOutOfRange, "GetRow" },
        { "GetRow below", [](MaxMatrix& m) { MaxMatrixRow row; return m.GetRow(MAXMATRIX_ROW_NUMBER_OF_MODULE, row).GetError(); }, MaxMatrixError::kOutOfRange, "GetRow" },
        { "GetRow no module", [](MaxMatrix& m) { return m.GetRow(MAXMATRIX_NUMBER_OF_MODULES, 0).GetError(); }, MaxMatrixError::kOutOfRange, "GetRow" },
        { "GetRow off its module", [](MaxMatrix& m) { return m.GetRow(0, MAXMATRIX_ROW_NUMBER_OF_MODULE).GetError(); }, MaxMatrixError::kOutOfRange, "GetRow" },
        { "GetDot left", [](MaxMatrix& m) { return m.GetDot(-1, 0).GetError(); }, MaxMatrixError::kOutOfRange, "GetDot" },
        { "GetDot right", [](MaxMatrix& m) { return m.GetDot(MAXMATRIX_NUMBER_OF_COLUMNS, 0).GetError(); }, MaxMatrixError::kOutOfRange, "GetDot" },
        { "GetDot above", [](MaxMatrix& m) { return m.GetDot(0, -1).GetError(); }, MaxMatrixError::kOutOfRange, "GetDot" },
        { "GetDot below", [](MaxMatrix& m) { return m.GetDot(0, MAXMATRIX_ROW_NUMBER_OF_MODULE).GetError(); }, MaxMatrixError::kOutOfRange, "GetDot" },
        { "GetSprite", [](MaxMatrix& m) { MaxMatrixSprite sprite; return m.GetSprite(255, &sprite).GetError(); }, MaxMatrixError::kOutOfRange, "GetSprite" },
        { "SetIntensity", [](MaxMatrix& m) { return m.SetIntensity(MAX7219_REG_INTENSITY_MAX_VALUE + 1).GetError(); }, MaxMatrixError::kOutOfRange, "SetIntensity" },
        { "SetChar", [](MaxMatrix& m) { return m.SetChar(0, 0, '\x01').GetError(); }, MaxMatrixError::kUnknownChar, "SetChar" },
        { "SetText unknown char", [](MaxMatrix& m) { return m.SetText("\x01").GetError(); }, MaxMatrixError::kUnknownChar, "SetText" },
        { "SetText NULL", [](MaxMatrix& m) { return m.SetText(NULL).GetError(); }, MaxMatrixError::kNoText, "SetText" },
        { "SetCharWithShift", [](MaxMatrix& m) { return m.SetCharWithShift('\x01').GetError(); }, MaxMatrixError::kUnknownChar, "SetCharWithShift" },
        { "SetTextWithShift NULL", [](MaxMatrix& m) { return m.SetTextWithShift(NULL).GetError(); }, MaxMatrixError::kNoText, "SetTextWithShift" },
        { "number empty", [](MaxMatrix&) { return MaxMatrixCommand::ConvertToNumber("", 255).GetError(); }, MaxMatrixError::kNotANumber, NULL },
        { "number with a letter", [](MaxMatrix&) { return MaxMatrixCommand::ConvertToNumber("12a", 255).GetError(); }, MaxMatrixError::kNotANumber, NULL },
        { "number too large", [](MaxMatrix&) { return MaxMatrixCommand::ConvertToNumber("256", 255).GetError(); }, MaxMatrixError::kOutOfRange, NULL },
        { "number too large, then a letter", [](MaxMatrix&) { return MaxMatrixCommand::ConvertToNumber("999x", 255).GetError(); }, MaxMatrixError::kNotANumber, NULL },
        { "unknown command", [](MaxMatrix& m) { return FeedText(m, "blink\n"); }, MaxMatrixError::kUnknownCommand, NULL },
        { "line too long", [](MaxMatrix& m) { return FeedText(m, "text=" + std::string(MAXMATRIX_COMMAND_LINE_LENGTH, 'x') + "\n"); }, MaxMatrixError::kLineTooLong, NULL },
    };

    for(auto const& test_case : cases) {
        matrix_.Clear();
        int32_t frames_before = mock::frames;
        byte intensity_before = ChipOfModule(0).intensity;
        reports.clear();
        EXPECT_EQ(test_case.call(matrix_), test_case.expected) << test_case.name;
        EXPECT_EQ(ShownPicture(matrix_), Picture([](int16_t, int16_t) { return false; })) << test_case.name;
        EXPECT_EQ(mock::frames, frames_before) << test_case.name;
        EXPECT_EQ(ChipOfModule(0).intensity, intensity_before) << test_case.name;
        EXPECT_EQ(matrix_.GetState(), MaxMatrixState::kReady) << test_case.name;
        std::vector<Report> expected;
        if(kDevErrorDetect && test_case.api != NULL) expected.push_back(Report(test_case.api, test_case.expected));
        EXPECT_EQ(reports, expected) << test_case.name;
    }
}

/* every error has its text, as SerialControl answers with it, and a byte off the enum gets one as well; the texts
 * lie in a table of their own, so one moved by a place would give every error after it the text of the next */
TEST(MaxMatrixErrorTest, EveryErrorHasItsText)
{
    char const* const texts[] = {
        "ok", "out of range", "unknown char", "no text", "not a number", "unknown command", "line too long"
    };
    ASSERT_EQ(sizeof(texts) / sizeof(texts[0]), kNumberOfErrors);

    for(uint8_t error = 0; error < kNumberOfErrors; error++) {
        EXPECT_EQ(Text(MaxMatrixStatus(static_cast<MaxMatrixError>(error)).GetErrorText()), texts[error]) << kErrorNames[error];
    }
    EXPECT_EQ(Text(MaxMatrixResult<byte>(MaxMatrixError::kUnknownChar).GetErrorText()), "unknown char");
    /* a new error that kErrorNames lacks has a text of its own here, and fails */
    EXPECT_EQ(Text(MaxMatrixStatus(static_cast<MaxMatrixError>(kNumberOfErrors)).GetErrorText()), "unknown error");
}

#if(MAXMATRIX_DEV_ERROR_DETECT == STD_ON)
/* the default report prints what the tests record instead, so the printing is a function of its own and tested here */
TEST(MaxMatrixErrorTest, PrintedReportNamesFunctionAndError)
{
    mock::serial_output.clear();
    MaxMatrixPrintError(F("SetText"), MaxMatrixError::kUnknownChar);
    EXPECT_EQ(mock::serial_output, "MaxMatrix SetText: unknown char\r\n");
}
#endif

/* drawing off the panel leaves the panel as it was and sends nothing; column 8 of module 0 used to be checked
 * against the panel alone, which would have drawn it on module 1 */
TEST_F(MaxMatrixTest, DrawingOffThePanelIsLeftOut)
{
    MaxMatrixRow row = { 0xFF, 0xFF, 0xFF, 0xFF };
    struct {
        char const* name;
        void (*draw)(MaxMatrix&, MaxMatrixRow const);
    } const cases[] = {
        { "SetDot left", [](MaxMatrix& m, MaxMatrixRow const) { m.SetDot(-1, 0, true); } },
        { "SetDot right", [](MaxMatrix& m, MaxMatrixRow const) { m.SetDot(MAXMATRIX_NUMBER_OF_COLUMNS, 0, true); } },
        { "SetDot above", [](MaxMatrix& m, MaxMatrixRow const) { m.SetDot(0, -1, true); } },
        { "SetDot below", [](MaxMatrix& m, MaxMatrixRow const) { m.SetDot(0, MAXMATRIX_ROW_NUMBER_OF_MODULE, true); } },
        { "SetColumn left", [](MaxMatrix& m, MaxMatrixRow const) { m.SetColumn(-1, 0xFF); } },
        { "SetColumn right", [](MaxMatrix& m, MaxMatrixRow const) { m.SetColumn(MAXMATRIX_NUMBER_OF_COLUMNS, 0xFF); } },
        { "SetColumn no module", [](MaxMatrix& m, MaxMatrixRow const) { m.SetColumn(MAXMATRIX_NUMBER_OF_MODULES, 0, 0xFF); } },
        { "SetColumn off its module", [](MaxMatrix& m, MaxMatrixRow const) { m.SetColumn(0, MAXMATRIX_COLUMN_NUMBER_OF_MODULE, 0xFF); } },
        { "SetRow above", [](MaxMatrix& m, MaxMatrixRow const r) { m.SetRow(-1, r); } },
        { "SetRow below", [](MaxMatrix& m, MaxMatrixRow const r) { m.SetRow(MAXMATRIX_ROW_NUMBER_OF_MODULE, r); } },
        { "SetRow no module", [](MaxMatrix& m, MaxMatrixRow const) { m.SetRow(MAXMATRIX_NUMBER_OF_MODULES, 0, 0xFF); } },
        { "SetRow off its module", [](MaxMatrix& m, MaxMatrixRow const) { m.SetRow(0, MAXMATRIX_ROW_NUMBER_OF_MODULE, 0xFF); } },
    };

    for(auto const& test_case : cases) {
        matrix_.Clear();
        int32_t frames_before = mock::frames;
        test_case.draw(matrix_, row);
        EXPECT_EQ(ShownPicture(matrix_), Picture([](int16_t, int16_t) { return false; })) << test_case.name;
        EXPECT_EQ(mock::frames, frames_before) << test_case.name;
        EXPECT_TRUE(reports.empty()) << test_case.name << " reported an error";
    }
}

/* a char partly off the panel shows the part on it, through either path of SetSprite(): whole columns for a char
 * at the top, single dots for one moved down */
TEST_P(MaxMatrixOrientationTest, CharPartlyOffThePanelShowsThePartOnIt)
{
    MaxMatrixSprite sprite;
    ASSERT_EQ(MaxMatrixFont::GetCharSprite('A', &sprite).GetError(), MaxMatrixError::kOk);
    int16_t const width = sprite[kSpriteWidth], height = sprite[kSpriteHeight];
    struct {
        int16_t x, y;
    } const places[] = { { -2, 0 }, { MAXMATRIX_NUMBER_OF_COLUMNS - 2, 0 }, { -2, -3 }, { MAXMATRIX_NUMBER_OF_COLUMNS - 2, 5 } };

    for(auto const& place : places) {
        matrix_.Clear();
        ASSERT_EQ(matrix_.SetChar(place.x, place.y, 'A').GetError(), MaxMatrixError::kOk);
        EXPECT_EQ(ShownPicture(matrix_), Picture([&sprite, &place, width, height](int16_t x, int16_t y) {
            int16_t sprite_x = x - place.x, sprite_y = y - place.y;
            return sprite_x >= 0 && sprite_x < width && sprite_y >= 0 && sprite_y < height &&
                   bitRead(sprite[kSpriteColumn1 + sprite_x], sprite_y);
        })) << "X " << place.x << ", Y " << place.y;
    }
}

/* a changed column of a module goes out in one frame with the same column of every other module */
TEST_F(MaxMatrixTest, ChangedColumnIsSentForEveryModule)
{
    byte const module = MAXMATRIX_NUMBER_OF_MODULES - 2;
    int32_t frames_before = mock::frames;

    matrix_.panel_.wired_.StoreBit(module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE + 3, 1, true);
    matrix_.Update();
    EXPECT_EQ(mock::frames - frames_before, 1);
    ASSERT_EQ(mock::last_frame.size(), 2u * MAXMATRIX_NUMBER_OF_MODULES);
    for(byte i = 0; i < MAXMATRIX_NUMBER_OF_MODULES; i++) {
        SCOPED_TRACE(::testing::Message() << "Module " << static_cast<uint16_t>(i));
        EXPECT_EQ(mock::last_frame[2 * i], MAX7219_REG_DIGIT3_ADDRESS);
        EXPECT_EQ(mock::last_frame[2 * i + 1], i == module ? 0x02 : 0x00);
    }
}

/* Clear() has to reach LEDs the buffer knows nothing of: after power-up the registers hold anything while the buffer
 * is empty, so a clear that sent only what changed in the buffer would send nothing */
TEST_F(MaxMatrixTest, InitClearsWhatThePowerUpLeft)
{
    mock::Reset(kDataInPin, kChipSelectPin, kClockPin, MAXMATRIX_NUMBER_OF_MODULES);
    for(Max7219& chip : mock::chain.chips) {
        for(byte digit = 0; digit < Max7219::kNumberOfDigits; digit++) chip.digit[digit] = 0xA5;
    }
    matrix_.Init();
    EXPECT_EQ(Display(), std::vector<byte>(MAXMATRIX_NUMBER_OF_COLUMNS, 0));
}

TEST_F(MaxMatrixTest, DrawingWhatIsShownSendsNothing)
{
    matrix_.SetText("Hi!");
    int32_t frames_before = mock::frames;

    matrix_.SetText("Hi!");
    matrix_.SetDot(0, 0, matrix_.panel_.wired_.GetColumn(0) & 1);
    EXPECT_EQ(mock::frames, frames_before);
}

/* the same drawing in both modes: a frame for every call that changed something, or none until Update(), which sends
 * each changed column of a module once */
TEST_F(MaxMatrixTest, DrawingInBothModes)
{
    std::vector<byte> drawn;
    int32_t frames_before = mock::frames;

    for(byte column = 0; column < MAXMATRIX_NUMBER_OF_COLUMNS; column++) matrix_.SetDot(column, column % MAXMATRIX_ROW_NUMBER_OF_MODULE, true);
    EXPECT_EQ(mock::frames - frames_before, MAXMATRIX_NUMBER_OF_COLUMNS);
    drawn = Display();

    matrix_.Clear();
    matrix_.SetAutoUpdate(false);
    frames_before = mock::frames;
    for(byte column = 0; column < MAXMATRIX_NUMBER_OF_COLUMNS; column++) matrix_.SetDot(column, column % MAXMATRIX_ROW_NUMBER_OF_MODULE, true);
    EXPECT_EQ(mock::frames, frames_before);
    EXPECT_EQ(Display(), std::vector<byte>(MAXMATRIX_NUMBER_OF_COLUMNS, 0)) << "shown before Update()";

    matrix_.Update();
    EXPECT_EQ(mock::frames - frames_before, MAXMATRIX_COLUMN_NUMBER_OF_MODULE);
    EXPECT_EQ(Display(), drawn);
}

TEST_F(MaxMatrixTest, AutoUpdateOnSendsWhatWasDrawn)
{
    matrix_.SetAutoUpdate(false);
    matrix_.SetText("Hi!");
    EXPECT_EQ(Display(), std::vector<byte>(MAXMATRIX_NUMBER_OF_COLUMNS, 0));

    matrix_.SetAutoUpdate(true);
    EXPECT_NE(Display(), std::vector<byte>(MAXMATRIX_NUMBER_OF_COLUMNS, 0));
}

TEST_F(MaxMatrixTest, BatchSendsAtTheEndOfTheBlock)
{
    int32_t frames_before = mock::frames;

    {
        MaxMatrixBatch batch(matrix_);
        for(byte column = 0; column < MAXMATRIX_NUMBER_OF_COLUMNS; column++) matrix_.SetDot(column, column % MAXMATRIX_ROW_NUMBER_OF_MODULE, true);
        EXPECT_FALSE(matrix_.GetAutoUpdate());
        EXPECT_EQ(mock::frames, frames_before);
        EXPECT_EQ(Display(), std::vector<byte>(MAXMATRIX_NUMBER_OF_COLUMNS, 0)) << "shown inside the block";
    }
    EXPECT_TRUE(matrix_.GetAutoUpdate());
    EXPECT_EQ(mock::frames - frames_before, MAXMATRIX_COLUMN_NUMBER_OF_MODULE);
}

/* the inner batch puts back the state it found, which the outer one had turned off */
TEST_F(MaxMatrixTest, NestedBatchesSendAtTheOutermost)
{
    int32_t frames_before = mock::frames;

    {
        MaxMatrixBatch outer(matrix_);
        {
            MaxMatrixBatch inner(matrix_);
            matrix_.SetDot(1, 1, true);
        }
        EXPECT_EQ(mock::frames, frames_before) << "sent at the end of the inner block";
        matrix_.SetDot(2, 2, true);
    }
    EXPECT_GT(mock::frames, frames_before);
}

/* a batch inside a sketch that sends with Update() itself leaves it that way */
TEST_F(MaxMatrixTest, BatchKeepsAutoUpdateOff)
{
    matrix_.SetAutoUpdate(false);
    int32_t frames_before = mock::frames;

    {
        MaxMatrixBatch batch(matrix_);
        matrix_.SetDot(1, 1, true);
    }
    EXPECT_FALSE(matrix_.GetAutoUpdate());
    EXPECT_EQ(mock::frames, frames_before);
    matrix_.Update();
}

TEST_F(MaxMatrixTest, ShiftIntervalStartsAtTheConfiguredValue)
{
    MaxMatrix fresh(kDataInPin, kChipSelectPin, kClockPin);

    EXPECT_EQ(fresh.GetShiftInterval(), MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE);
}

/* The tests below count the steps of a shifting char in sprite_shift_counter_, which CharShiftTask() moves on by one per
 * step, and hold the clock, so that a step 50 ms on does not depend on how fast the host is. */

/* the first step comes at once, every further one an interval after the step before */
TEST_F(MaxMatrixTest, ShiftWaitsForTheInterval)
{
    mock::SetMillis(1000);
    matrix_.SetShiftInterval(50);
    ASSERT_EQ(matrix_.SetCharWithShift('#').GetError(), MaxMatrixError::kOk);
    int16_t const start = matrix_.text_.sprite_shift_counter_;

    matrix_.ShiftTask();
    EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 1) << "no first step at once";
    mock::SetMillis(1049);
    matrix_.ShiftTask();
    EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 1) << "a step before the interval had passed";
    mock::SetMillis(1050);
    matrix_.ShiftTask();
    EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 2);
}

/* a call after loop() was held up takes one step, not the ones it missed, and the interval counts from there */
TEST_F(MaxMatrixTest, LateShiftDoesNotCatchUp)
{
    mock::SetMillis(1000);
    matrix_.SetShiftInterval(50);
    ASSERT_EQ(matrix_.SetCharWithShift('#').GetError(), MaxMatrixError::kOk);
    int16_t const start = matrix_.text_.sprite_shift_counter_;

    matrix_.ShiftTask();
    mock::SetMillis(1180);
    matrix_.ShiftTask();
    matrix_.ShiftTask();
    EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 2);
    mock::SetMillis(1229);
    matrix_.ShiftTask();
    EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 2);
    mock::SetMillis(1230);
    matrix_.ShiftTask();
    EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 3);
}

TEST_F(MaxMatrixTest, ShiftIntervalZeroStepsOnEveryCall)
{
    mock::SetMillis(1000);
    matrix_.SetShiftInterval(0);
    ASSERT_EQ(matrix_.SetCharWithShift('#').GetError(), MaxMatrixError::kOk);
    int16_t const start = matrix_.text_.sprite_shift_counter_;

    for(uint8_t call = 0; call < 3; call++) matrix_.ShiftTask();
    EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 3);
}

/* a new char or text starts at once, however recently the one before took a step */
TEST_F(MaxMatrixTest, NewShiftStartsAtOnce)
{
    mock::SetMillis(1000);
    matrix_.SetShiftInterval(50);
    ASSERT_EQ(matrix_.SetCharWithShift('#').GetError(), MaxMatrixError::kOk);
    matrix_.ShiftTask();

    mock::SetMillis(1010);
    ASSERT_EQ(matrix_.SetCharWithShift('+').GetError(), MaxMatrixError::kOk);
    int16_t const start = matrix_.text_.sprite_shift_counter_;
    matrix_.ShiftTask();
    EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 1);
}

/* chip select was not an output in SPI mode, and the SPI transaction was never ended */
TEST_F(MaxMatrixTest, TransfersAreFramed)
{
    char text[] = "Hello";

    EXPECT_EQ(mock::pin_mode[kChipSelectPin], OUTPUT);
    matrix_.SetText("Hi!");
    matrix_.SetModuleOrientation(MaxMatrixModuleOrientation::k90);
    ShiftWholeText(text);

    EXPECT_GT(mock::frames, 0);
    EXPECT_EQ(mock::errors, 0);
    EXPECT_EQ(mock::pin_level[kChipSelectPin], HIGH);
    EXPECT_FALSE(mock::transaction_open);
    EXPECT_EQ(mock::UsesSpi(), MAXMATRIX_USE_SPI == STD_ON);
}

TEST_F(MaxMatrixTest, TextShiftFinishes)
{
    char text[] = "Hello World!";

    ShiftWholeText(text);
    EXPECT_EQ(matrix_.GetState(), MaxMatrixState::kReady);
}

/* the Arduino IDE saves sketches as UTF-8, so an umlaut arrives as two bytes */
TEST_F(MaxMatrixTest, UmlautsFromUtf8AndLatin1Match)
{
    std::vector<byte> utf8, latin1;

    matrix_.Clear();
    EXPECT_EQ(matrix_.SetText("Äöü!").GetError(), MaxMatrixError::kOk);
    utf8 = Bytes(matrix_.panel_.wired_.matrix_buffer_, sizeof(matrix_.panel_.wired_.matrix_buffer_));
    matrix_.Clear();
    EXPECT_EQ(matrix_.SetText("\xC4\xF6\xFC!").GetError(), MaxMatrixError::kOk);
    latin1 = Bytes(matrix_.panel_.wired_.matrix_buffer_, sizeof(matrix_.panel_.wired_.matrix_buffer_));
    EXPECT_EQ(utf8, latin1);

    char shift_utf8[] = "Grüße Ä";
    char shift_latin1[] = "Gr\xFC\xDF" "e \xC4";
    ShiftWholeText(shift_utf8);
    EXPECT_EQ(matrix_.GetState(), MaxMatrixState::kReady);
    utf8 = Bytes(matrix_.panel_.wired_.matrix_buffer_, sizeof(matrix_.panel_.wired_.matrix_buffer_));
    ShiftWholeText(shift_latin1);
    latin1 = Bytes(matrix_.panel_.wired_.matrix_buffer_, sizeof(matrix_.panel_.wired_.matrix_buffer_));
    EXPECT_EQ(utf8, latin1);
}

TEST_F(MaxMatrixTest, EveryCharBeyondAsciiHasItsOwnSprite)
{
    char const* chars[] = { "Ä", "Ö", "Ü", "ä", "ö", "ü", "ß", "°" };
    char c;

    for(byte i = 0; i < 8; i++) {
        SCOPED_TRACE(chars[i]);
        EXPECT_EQ(MaxMatrixFont::DecodeChar(chars[i], &c), 2);
        MaxMatrixResult<MaxMatrixSpriteIndex> sprite_index = MaxMatrixFont::ConvertCharToSprite(c);
        EXPECT_EQ(sprite_index.GetError(), MaxMatrixError::kOk);
        EXPECT_EQ(sprite_index.GetValue(), 95 + i);
    }

    /* the Ü sprite used to be a copy of the O */
    MaxMatrixSprite ue, o;
    EXPECT_EQ(matrix_.GetSprite(97, &ue).GetError(), MaxMatrixError::kOk);
    EXPECT_EQ(matrix_.GetSprite('O' + MAXMATRIX_ASCII_TABLE_OFFSET, &o).GetError(), MaxMatrixError::kOk);
    EXPECT_NE(Bytes(ue, sizeof(ue)), Bytes(o, sizeof(o)));
}

/* Bug: the font had no ß and no °, so SetText() failed on them and a shifting text, "Grüße" or "21 °C", lost them
 * without a word */
TEST_F(MaxMatrixTest, EszettAndDegreeAreDrawn)
{
    char const* texts[] = { "ß", "°", "\xDF", "\xB0" };

    for(char const* text : texts) {
        SCOPED_TRACE(text);
        matrix_.Clear();
        EXPECT_EQ(matrix_.SetText(text).GetError(), MaxMatrixError::kOk);
        std::vector<byte> const shown = Display();
        EXPECT_TRUE(std::any_of(shown.begin(), shown.end(), [](byte column) { return column != 0; }));
    }
}

TEST_F(MaxMatrixTest, DecodeStopsAtEndOfString)
{
    char c;

    EXPECT_EQ(MaxMatrixFont::DecodeChar("A", &c), 1);
    EXPECT_EQ(c, 'A');
    EXPECT_EQ(MaxMatrixFont::DecodeChar("\xC3", &c), 1);

    char truncated[] = "a\xC3";
    ShiftWholeText(truncated);
    EXPECT_EQ(matrix_.GetState(), MaxMatrixState::kReady);
}

/* an unknown char used to be shifted in all the same, with whatever sprite was left over; that the
 * shift finishes does not show it, so what went to the chain is compared with the text without it.
 * Skipping it then took a ShiftTask() call of its own that sent nothing, and the text stood still for
 * a step, so the calls are compared as well - in the middle of the text and at its end. */
TEST_F(MaxMatrixTest, UnknownChars)
{
    EXPECT_EQ(matrix_.SetText("a\x01").GetError(), MaxMatrixError::kUnknownChar);

    char known[] = "ab";
    int32_t frames = ShiftWholeText(known);
    uint16_t calls = shift_calls_;

    char in_the_middle[] = "a\x01\x02" "b";
    EXPECT_EQ(ShiftWholeText(in_the_middle), frames);
    EXPECT_EQ(shift_calls_, calls);
    EXPECT_EQ(matrix_.GetState(), MaxMatrixState::kReady);

    char at_the_end[] = "ab\x01";
    EXPECT_EQ(ShiftWholeText(at_the_end), frames);
    EXPECT_EQ(shift_calls_, calls);
    EXPECT_EQ(matrix_.GetState(), MaxMatrixState::kReady);
}

class MaxMatrixCommandTest : public MaxMatrixTest {
  protected:
    MaxMatrixCommand command_;
    TextPrint answer_;

    /* feeds a line char by char, as Serial would; every char but the last has to leave the line pending, and answer
     * nothing */
    MaxMatrixResult<MaxMatrixCommandState> FeedLine(std::string const& text)
    {
        answer_.text.clear();
        for(size_t i = 0; i + 1 < text.size(); i++) {
            EXPECT_EQ(command_.Feed(matrix_, text[i], answer_), MaxMatrixCommandState::kPending) << "char " << i << " of " << text;
        }
        EXPECT_EQ(answer_.text, "") << "answered before the end of " << text;
        return command_.Feed(matrix_, text.back(), answer_);
    }

    /* the display after every step of the shift running now, until it is done */
    std::vector<std::vector<byte>> ShiftSteps()
    {
        std::vector<std::vector<byte>> steps;
        for(uint16_t step = 0; step < 1000 && matrix_.GetState() != MaxMatrixState::kReady; step++) {
            matrix_.ShiftTask();
            steps.push_back(Display());
        }
        return steps;
    }
};

TEST_F(MaxMatrixCommandTest, TextShowsWhatSetTextShows)
{
    matrix_.SetText("xx");
    ASSERT_EQ(FeedLine("text=Hi!\n"), MaxMatrixCommandState::kDone);
    std::vector<byte> const shown = Display();

    matrix_.Clear();
    matrix_.SetText("Hi!");
    EXPECT_EQ(shown, Display());
}

TEST_F(MaxMatrixCommandTest, SettingsAndDotsReachTheMatrix)
{
    EXPECT_EQ(FeedLine("intensity=8\n"), MaxMatrixCommandState::kDone);
    EXPECT_EQ(ChipOfModule(0).intensity, 8);
    EXPECT_EQ(FeedLine("speed=120\n"), MaxMatrixCommandState::kDone);
    EXPECT_EQ(matrix_.GetShiftInterval(), 120);
    EXPECT_EQ(FeedLine("space=3\n"), MaxMatrixCommandState::kDone);
    EXPECT_EQ(matrix_.GetSpaceBetweenChars(), 3);

    EXPECT_EQ(FeedLine("setDot=5,3,1\n"), MaxMatrixCommandState::kDone);
    EXPECT_EQ(matrix_.GetDot(5, 3).GetError(), MaxMatrixError::kOk);
    EXPECT_TRUE(matrix_.GetDot(5, 3).GetValue());
    EXPECT_EQ(FeedLine("setDot=5,3,0\n"), MaxMatrixCommandState::kDone);
    EXPECT_EQ(matrix_.GetDot(5, 3).GetError(), MaxMatrixError::kOk);
    EXPECT_FALSE(matrix_.GetDot(5, 3).GetValue());

    matrix_.SetText("Hi");
    EXPECT_EQ(FeedLine("clear\n"), MaxMatrixCommandState::kDone);
    EXPECT_EQ(Display(), std::vector<byte>(MAXMATRIX_NUMBER_OF_COLUMNS, 0));
}

/* SetTextWithShift() keeps only a pointer: a shift that read its text out of the line buffer would shift whatever
 * line came next */
TEST_F(MaxMatrixCommandTest, TextShiftKeepsItsTextWhileLinesFollow)
{
    char text[] = "AB";

    ASSERT_EQ(FeedLine("textShift=AB\n"), MaxMatrixCommandState::kDone);
    ASSERT_EQ(FeedLine("intensity=3\n"), MaxMatrixCommandState::kDone);
    std::vector<std::vector<byte>> const commanded = ShiftSteps();

    matrix_.Clear();
    ASSERT_EQ(matrix_.SetTextWithShift(text).GetError(), MaxMatrixError::kOk);
    EXPECT_EQ(commanded, ShiftSteps());
}

TEST_F(MaxMatrixCommandTest, WrongLinesAreRefused)
{
    struct {
        char const* line;
        MaxMatrixError error;
    } const cases[] = {
        { "blink\n", MaxMatrixError::kUnknownCommand },
        { "clear=1\n", MaxMatrixError::kUnknownCommand },
        { "intensity=16\n", MaxMatrixError::kOutOfRange },
        { "intensity=\n", MaxMatrixError::kNotANumber },
        { "intensity=x\n", MaxMatrixError::kNotANumber },
        { "speed=65536\n", MaxMatrixError::kOutOfRange },
        { "speed=99999999999999999999\n", MaxMatrixError::kOutOfRange },
        /* 2^32, which a count in 32 bits wraps round to 0, a speed it would take */
        { "speed=4294967296\n", MaxMatrixError::kOutOfRange },
        { "space=-1\n", MaxMatrixError::kNotANumber },
        { "setDot=1,2\n", MaxMatrixError::kNotANumber },
        { "setDot=32,0,1\n", MaxMatrixError::kOutOfRange },
        { "setDot=1,8,1\n", MaxMatrixError::kOutOfRange },
        { "setDot=1,2,2\n", MaxMatrixError::kOutOfRange },
        { "textShift=\n", MaxMatrixError::kNoText },
        { "text=a\x01\n", MaxMatrixError::kUnknownChar },
    };

    for(auto const& test_case : cases) EXPECT_EQ(FeedLine(test_case.line), test_case.error) << test_case.line;
    EXPECT_EQ(matrix_.GetShiftInterval(), 0) << "changed by a refused speed=";
}

TEST_F(MaxMatrixCommandTest, LineEndsInNewlineOrCarriageReturnNewline)
{
    EXPECT_EQ(FeedLine("intensity=4\r\n"), MaxMatrixCommandState::kDone);
    EXPECT_EQ(ChipOfModule(0).intensity, 4);
    EXPECT_EQ(FeedLine("\n"), MaxMatrixCommandState::kPending) << "an empty line";
    EXPECT_EQ(FeedLine("intensity=5\n"), MaxMatrixCommandState::kDone);
}

/* a line cut short would carry out a different command, so a line too long is dropped whole; the next one is read */
TEST_F(MaxMatrixCommandTest, LineTooLongIsDroppedWhole)
{
    std::string const long_text = "text=" + std::string(MAXMATRIX_COMMAND_LINE_LENGTH, 'x') + "\n";

    EXPECT_EQ(FeedLine(long_text), MaxMatrixError::kLineTooLong);
    EXPECT_EQ(Display(), std::vector<byte>(MAXMATRIX_NUMBER_OF_COLUMNS, 0));
    EXPECT_EQ(FeedLine("intensity=2\n"), MaxMatrixCommandState::kDone);
    EXPECT_EQ(ChipOfModule(0).intensity, 2);
}

/* a program pairs every line it sent with one line back, so each line is answered once, an empty line not at all */
TEST_F(MaxMatrixCommandTest, EveryLineIsAnsweredOnce)
{
    struct {
        char const* line;
        char const* answer;
    } const cases[] = {
        { "intensity=8\n", "ok\r\n" },
        { "clear\r\n", "ok\r\n" },
        { "intensity=16\n", "error: out of range\r\n" },
        { "blink\n", "error: unknown command\r\n" },
        { "\n", "" },
        { "\r\n", "" },
    };

    for(auto const& test_case : cases) {
        FeedLine(test_case.line);
        EXPECT_EQ(answer_.text, test_case.answer) << test_case.line;
    }
    FeedLine("text=" + std::string(MAXMATRIX_COMMAND_LINE_LENGTH, 'x') + "\n");
    EXPECT_EQ(answer_.text, "error: line too long\r\n");
}

/* picture answers in place of ok, and changes nothing */
TEST_F(MaxMatrixCommandTest, PictureIsAnsweredWithTheColumns)
{
    matrix_.SetText("Hi");
    std::vector<byte> const shown = Display();

    EXPECT_EQ(FeedLine("picture\n"), MaxMatrixCommandState::kAnswered);
    ASSERT_EQ(answer_.text.size(), 8 + 2 * MAXMATRIX_NUMBER_OF_COLUMNS + 2);
    EXPECT_EQ(answer_.text.substr(0, 8), "picture=");
    EXPECT_EQ(answer_.text.substr(answer_.text.size() - 2), "\r\n");
    EXPECT_EQ(Display(), shown);
    EXPECT_EQ(FeedLine("picture=1\n"), MaxMatrixError::kUnknownCommand);
}

TEST_F(MaxMatrixCommandTest, LineOfFullLengthIsTaken)
{
    std::string const text = "text=" + std::string(MAXMATRIX_COMMAND_LINE_LENGTH - 5, 'x') + "\n";

    EXPECT_EQ(FeedLine(text), MaxMatrixCommandState::kDone);
}

/* picture answers with the panel as the viewer sees it, so that a program draws it without knowing the orientations;
 * one dot in a column of every module, each in another row */
TEST_P(MaxMatrixOrientationTest, PictureAnswerIsWhatTheViewerSees)
{
    MaxMatrixCommand command;
    TextPrint answer;
    std::string expected = "picture=";

    matrix_.SetDot(0, 0, true);
    matrix_.SetDot(9, 1, true);
    matrix_.SetDot(16, 4, true);
    matrix_.SetDot(31, 7, true);
    for(char c : std::string("picture\n")) command.Feed(matrix_, c, answer);
    for(byte column = 0; column < MAXMATRIX_NUMBER_OF_COLUMNS; column++) {
        expected += column == 0 ? "01" : column == 9 ? "02" : column == 16 ? "10" : column == 31 ? "80" : "00";
    }
    EXPECT_EQ(answer.text, expected + "\r\n");
}
