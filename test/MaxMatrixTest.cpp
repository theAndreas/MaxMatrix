/* Host-side tests of MaxMatrix against the Arduino stand-in in mock/.
 *
 * Built twice by CMakeLists.txt, once with bit banging and once with MAXMATRIX_USE_SPI=STD_ON, because the
 * two transfer paths are separate code. What this cannot show is anything about timing or the electrical
 * side - whether a real MAX7219 accepts the frames is only known on hardware. */
#include <gtest/gtest.h>
#include <string>
#include <tuple>
#include <vector>
#include "Arduino.h"

/* The parts of the library and the matrix buffer are private members of MaxMatrix, and some of what is
 * tested here has no public path: MaxMatrixWired::setColumnOnAllModules is only reached through clear(),
 * which zeroes the buffer afterwards and so hides a wrong index. Included after GoogleTest and the
 * standard headers, which must not see it - defined above them, it rewrites their classes too. */
#define private public
#include "MaxMatrix.h"
#undef private

static const byte DataInPin = 11;
static const byte ChipSelectPin = 10;
static const byte ClockPin = 13;

/* every error by name, in the order of the enum */
static const char* const ErrorNames[] = {
    "MAXMATRIX_E_OK", "MAXMATRIX_E_OUT_OF_RANGE", "MAXMATRIX_E_UNKNOWN_CHAR", "MAXMATRIX_E_NO_TEXT", "MAXMATRIX_E_NOT_A_NUMBER",
    "MAXMATRIX_E_UNKNOWN_COMMAND", "MAXMATRIX_E_LINE_TOO_LONG"
};
static const uint8_t NumberOfErrors = sizeof(ErrorNames) / sizeof(ErrorNames[0]);

/* GoogleTest would print an error as its number */
static void PrintTo(MaxMatrixErrorType Error, std::ostream* Out)
{
    if(Error < NumberOfErrors) *Out << ErrorNames[Error];
    else *Out << "error " << static_cast<uint16_t>(Error);
}

/* what feed() returns, compared with a state as with an error: a line is done only if it did not fail as well */
static bool operator==(const MaxMatrixResult<MaxMatrixCommandStateType>& Result, MaxMatrixCommandStateType State)
{
    return Result.isOk() && Result.getValue() == State;
}

static void PrintTo(const MaxMatrixResult<MaxMatrixCommandStateType>& Result, std::ostream* Out)
{
    if(!Result.isOk()) PrintTo(Result.getError(), Out);
    else *Out << (Result.getValue() == MAXMATRIX_COMMAND_DONE ? "MAXMATRIX_COMMAND_DONE" : "MAXMATRIX_COMMAND_PENDING");
}

/* the text of F() and getErrorText(), which lies in RAM on the host */
static std::string text(const __FlashStringHelper* Text)
{
    return reinterpret_cast<const char*>(Text);
}

/* a whole line to a command of its own, and why it failed */
static MaxMatrixErrorType feedText(MaxMatrix& Matrix, const std::string& Text)
{
    MaxMatrixCommand Command;
    MaxMatrixResult<MaxMatrixCommandStateType> Result = MAXMATRIX_COMMAND_PENDING;

    for(char Char : Text) Result = Command.feed(Matrix, Char);
    return Result.getError();
}

/* What the library reported, with MAXMATRIX_DEV_ERROR_DETECT on: the definition here wins over the weak one of the
 * library, which prints. With it off the list stays empty, so that a test can expect a report in every variant. */
typedef std::pair<std::string, MaxMatrixErrorType> ReportType;
static const bool DevErrorDetect = (MAXMATRIX_DEV_ERROR_DETECT == STD_ON);
static std::vector<ReportType> Reports;

#if(MAXMATRIX_DEV_ERROR_DETECT == STD_ON)
void maxMatrixReportError(const __FlashStringHelper* Api, MaxMatrixErrorType Error)
{
    Reports.push_back(ReportType(text(Api), Error));
}
#endif

/* GoogleTest prints a vector element by element, where a failed memcmp only says that something differs */
static std::vector<byte> bytes(const byte* Data, size_t Size)
{
    return std::vector<byte>(Data, Data + Size);
}

/* the library sends the pair of module 0 first, so it ends up in the chip at the far end of the chain */
static const Max7219& chipOfModule(byte Module)
{
    return Mock::Chain.Chips[MAXMATRIX_NUMBER_OF_MODULES - 1 - Module];
}

/* what the chain shows, laid out like MatrixBuffer: a column of a module is a digit, a row a segment */
static std::vector<byte> display()
{
    std::vector<byte> Columns;
    for(byte Module = 0; Module < MAXMATRIX_NUMBER_OF_MODULES; Module++) {
        for(byte Column = 0; Column < MAXMATRIX_COLUMN_NUMBER_OF_MODULE; Column++) {
            Columns.push_back(chipOfModule(Module).segments(Column));
        }
    }
    return Columns;
}

class MaxMatrixTest : public ::testing::Test {
  protected:
    MaxMatrix Matrix{DataInPin, ChipSelectPin, ClockPin};

    void SetUp() override
    {
        Mock::reset(ChipSelectPin, MAXMATRIX_NUMBER_OF_MODULES);
        Reports.clear();
        Matrix.init();
        /* the tests step a shifting text with every call; the timing of the steps has tests of its own */
        Matrix.setShiftInterval(0);
    }

    /* whatever a test did, the LEDs have to show what the buffer says - the get functions read the buffer, because
     * the MAX7219 cannot be read back, so a buffer that drifts from the display makes them lie */
    void TearDown() override
    {
        EXPECT_EQ(display(), bytes(Matrix.Panel.Wired.MatrixBuffer, sizeof(Matrix.Panel.Wired.MatrixBuffer))) << "display differs from MatrixBuffer";
    }

    /* the shiftTask() calls the last shiftWholeText() took, each one a step of the text on the panel */
    uint16_t ShiftCalls = 0;

    /* returns the number of frames the shift sent to the chain */
    int32_t shiftWholeText(char* Text)
    {
        Matrix.clear();
        int32_t FramesBefore = Mock::Frames;
        Matrix.setTextWithShift(Text);
        for(ShiftCalls = 0; ShiftCalls < 1000 && Matrix.getState() != MAXMATRIX_STATE_READY; ShiftCalls++) Matrix.shiftTask();
        return Mock::Frames - FramesBefore;
    }
};

static const MaxMatrixMatrixOrientationType MatrixOrientations[] = {
    MAXMATRIX_MATRIX_ORIENTATION_0, MAXMATRIX_MATRIX_ORIENTATION_180
};

static const MaxMatrixModuleOrientationType ModuleOrientations[] = {
    MAXMATRIX_MODULE_ORIENTATION_0, MAXMATRIX_MODULE_ORIENTATION_90,
    MAXMATRIX_MODULE_ORIENTATION_180, MAXMATRIX_MODULE_ORIENTATION_270
};

typedef std::tuple<MaxMatrixMatrixOrientationType, MaxMatrixModuleOrientationType> OrientationType;

/* every test of this fixture runs in all eight combinations of matrix and module orientation */
class MaxMatrixOrientationTest : public MaxMatrixTest, public ::testing::WithParamInterface<OrientationType> {
  protected:
    void SetUp() override
    {
        MaxMatrixTest::SetUp();
        Matrix.clear();
        Matrix.setMatrixOrientation(std::get<0>(GetParam()));
        Matrix.setModuleOrientation(std::get<1>(GetParam()));
    }
};

static std::string orientationName(const ::testing::TestParamInfo<OrientationType>& Info)
{
    static const char* const Names[] = { "0", "90", "180", "270" };
    return std::string("matrix") + (std::get<0>(Info.param) == MAXMATRIX_MATRIX_ORIENTATION_180 ? "180" : "0") +
           "_module" + Names[std::get<1>(Info.param)];
}

INSTANTIATE_TEST_SUITE_P(AllOrientations, MaxMatrixOrientationTest,
    ::testing::Combine(::testing::ValuesIn(MatrixOrientations), ::testing::ValuesIn(ModuleOrientations)),
    orientationName);

/* the bit position used to run on across modules, so only module 0 got its row */
TEST_P(MaxMatrixOrientationTest, RowRoundTrip)
{
    rowType In, Out;
    for(byte Module = 0; Module < MAXMATRIX_NUMBER_OF_MODULES; Module++) In[Module] = 0xA5 ^ (Module * 0x3C);

    for(byte Row = 0; Row < MAXMATRIX_ROW_NUMBER_OF_MODULE; Row++) {
        SCOPED_TRACE(::testing::Message() << "Row " << static_cast<uint16_t>(Row));
        Matrix.setRow(Row, In);
        EXPECT_EQ(Matrix.getRow(Row, Out).getError(), MAXMATRIX_E_OK);
        EXPECT_EQ(bytes(Out, sizeof(Out)), bytes(In, sizeof(In)));
    }
}

/* the display has to follow the buffer during the shift too, not only once the text has left it */
TEST_P(MaxMatrixOrientationTest, DisplayFollowsTheShift)
{
    char Text[] = "Hi \xC4!";

    Matrix.setTextWithShift(Text);
    for(uint16_t Step = 0; Step < 1000 && Matrix.getState() != MAXMATRIX_STATE_READY; Step++) {
        Matrix.shiftTask();
        ASSERT_EQ(display(), bytes(Matrix.Panel.Wired.MatrixBuffer, sizeof(Matrix.Panel.Wired.MatrixBuffer))) << "after step " << Step;
    }
}

/* Whatever a call draws, it sends one frame per changed column of a module at most. Every dot and column used to be a
 * frame of its own: setText("Hi!") in module orientation 270 sent 64, and each step of a shifting text nine. */
TEST_P(MaxMatrixOrientationTest, NoCallSendsMoreThanEightFrames)
{
    const struct {
        const char* Name;
        void (*Draw)(MaxMatrix&);
    } Calls[] = {
        { "setText", [](MaxMatrix& M) { M.setText("Hi!"); } },
        { "setChar off the top row", [](MaxMatrix& M) { M.setChar(9, 1, 'A'); } },
        { "setRow", [](MaxMatrix& M) { rowType Row = { 0xFF, 0x0F, 0xF0, 0x3C }; M.setRow(5, Row); } },
        { "setColumn", [](MaxMatrix& M) { M.setColumn(17, 0x5A); } },
        { "shiftUp", [](MaxMatrix& M) { M.shiftUp(true); } },
        { "clear", [](MaxMatrix& M) { M.clear(); } },
    };
    char Text[] = "Hi \xC4!";

    for(const auto& Call : Calls) {
        int32_t FramesBefore = Mock::Frames;
        Call.Draw(Matrix);
        EXPECT_LE(Mock::Frames - FramesBefore, MAXMATRIX_COLUMN_NUMBER_OF_MODULE) << Call.Name;
    }
    Matrix.setTextWithShift(Text);
    for(uint16_t Step = 0; Step < 1000 && Matrix.getState() != MAXMATRIX_STATE_READY; Step++) {
        int32_t FramesBefore = Mock::Frames;
        Matrix.shiftTask();
        ASSERT_LE(Mock::Frames - FramesBefore, MAXMATRIX_COLUMN_NUMBER_OF_MODULE) << "shift step " << Step;
    }
}

static bool pattern(int16_t X, int16_t Y)
{
    return (X * X + 3 * Y + X * Y) % 3 == 0;
}

/* a picture as rows of '#' and '.', so that a failure prints both pictures */
template<typename DotType>
static std::vector<std::string> picture(DotType Dot)
{
    std::vector<std::string> Rows;
    for(int16_t Y = 0; Y < MAXMATRIX_ROW_NUMBER_OF_MODULE; Y++) {
        std::string Row;
        for(int16_t X = 0; X < MAXMATRIX_NUMBER_OF_COLUMNS; X++) Row += Dot(X, Y) ? '#' : '.';
        Rows.push_back(Row);
    }
    return Rows;
}

/* the picture as the library reports it, through getDot */
static std::vector<std::string> shownPicture(MaxMatrix& Matrix)
{
    return picture([&Matrix](int16_t X, int16_t Y) { return Matrix.getDot(X, Y).getValue(); });
}

static void drawPattern(MaxMatrix& Matrix)
{
    for(byte X = 0; X < MAXMATRIX_NUMBER_OF_COLUMNS; X++) {
        for(byte Y = 0; Y < MAXMATRIX_ROW_NUMBER_OF_MODULE; Y++) Matrix.setDot(X, Y, pattern(X, Y));
    }
}

/* rows and columns are two views of one picture: in orientation 180 setColumn turned the whole panel while setRow
 * turned each module */
TEST_P(MaxMatrixOrientationTest, RowsAndColumnsAgree)
{
    rowType Row;
    for(byte Module = 0; Module < MAXMATRIX_NUMBER_OF_MODULES; Module++) Row[Module] = 0x81 ^ (Module * 0x16);

    Matrix.setRow(2, Row);
    EXPECT_EQ(shownPicture(Matrix), picture([&Row](int16_t X, int16_t Y) {
        return Y == 2 && bitRead(Row[X / MAXMATRIX_COLUMN_NUMBER_OF_MODULE], 7 - X % MAXMATRIX_COLUMN_NUMBER_OF_MODULE);
    }));

    Matrix.clear();
    Matrix.setColumn(5, 0xA3);
    EXPECT_EQ(shownPicture(Matrix), picture([](int16_t X, int16_t Y) { return X == 5 && bitRead(0xA3, Y); }));
    for(byte Y = 0; Y < MAXMATRIX_ROW_NUMBER_OF_MODULE; Y++) {
        MaxMatrixResult<byte> Value = Matrix.getRow(0, Y);
        EXPECT_EQ(Value.getError(), MAXMATRIX_E_OK);
        EXPECT_EQ(Value.getValue(), bitRead(0xA3, Y) ? 1 << (7 - 5) : 0) << "Row " << static_cast<uint16_t>(Y);
    }
}

/* every shift moves the whole picture by one dot as the viewer sees it, across the modules */
TEST_P(MaxMatrixOrientationTest, ShiftsMoveThePicture)
{
    const struct {
        const char* Name;
        void (*Shift)(MaxMatrix&);
        int16_t SourceX, SourceY;
        bool Rotate;
    } Cases[] = {
        { "shiftLeft", [](MaxMatrix& M) { M.shiftLeft(false, true); }, 1, 0, false },
        { "shiftLeft rotating", [](MaxMatrix& M) { M.shiftLeft(true, false); }, 1, 0, true },
        { "shiftRight", [](MaxMatrix& M) { M.shiftRight(false, true); }, -1, 0, false },
        { "shiftRight rotating", [](MaxMatrix& M) { M.shiftRight(true, false); }, -1, 0, true },
        { "shiftUp", [](MaxMatrix& M) { M.shiftUp(false); }, 0, 1, false },
        { "shiftUp rotating", [](MaxMatrix& M) { M.shiftUp(true); }, 0, 1, true },
        { "shiftDown", [](MaxMatrix& M) { M.shiftDown(false); }, 0, -1, false },
        { "shiftDown rotating", [](MaxMatrix& M) { M.shiftDown(true); }, 0, -1, true },
    };
    const int16_t Width = MAXMATRIX_NUMBER_OF_COLUMNS, Height = MAXMATRIX_ROW_NUMBER_OF_MODULE;

    for(const auto& Case : Cases) {
        Matrix.clear();
        drawPattern(Matrix);
        Case.Shift(Matrix);
        EXPECT_EQ(shownPicture(Matrix), picture([&Case, Width, Height](int16_t X, int16_t Y) {
            int16_t SourceX = X + Case.SourceX, SourceY = Y + Case.SourceY;
            if(Case.Rotate) return pattern((SourceX + Width) % Width, (SourceY + Height) % Height);
            return SourceX >= 0 && SourceX < Width && SourceY >= 0 && SourceY < Height && pattern(SourceX, SourceY);
        })) << Case.Name;
    }
}

/* changing an orientation turns the buffer along, so the viewer still sees the same picture; changing the module
 * orientation to or from 180 turned each module while setColumn had turned the whole panel */
TEST_P(MaxMatrixOrientationTest, PictureStaysWhenTheOrientationChanges)
{
    for(MaxMatrixMatrixOrientationType MatrixOrientation : MatrixOrientations) {
        for(MaxMatrixModuleOrientationType ModuleOrientation : ModuleOrientations) {
            Matrix.setMatrixOrientation(std::get<0>(GetParam()));
            Matrix.setModuleOrientation(std::get<1>(GetParam()));
            Matrix.clear();
            drawPattern(Matrix);
            Matrix.setMatrixOrientation(MatrixOrientation);
            Matrix.setModuleOrientation(ModuleOrientation);
            EXPECT_EQ(shownPicture(Matrix), picture(pattern))
                << "to matrix " << MatrixOrientation << ", module " << ModuleOrientation;
        }
    }
}

TEST_P(MaxMatrixOrientationTest, DotRoundTrip)
{
    for(byte Column = 0; Column < MAXMATRIX_NUMBER_OF_COLUMNS; Column++) {
        for(byte Row = 0; Row < MAXMATRIX_ROW_NUMBER_OF_MODULE; Row++) {
            SCOPED_TRACE(::testing::Message() << "Column " << static_cast<uint16_t>(Column) << ", Row " << static_cast<uint16_t>(Row));
            bool Expected = (Column * 3 + Row) & 1;
            Matrix.setDot(Column, Row, Expected);
            MaxMatrixResult<bool> Value = Matrix.getDot(Column, Row);
            EXPECT_EQ(Value.getError(), MAXMATRIX_E_OK);
            EXPECT_EQ(Value.getValue(), Expected);
        }
    }
}

/* Which orientations each module needs, as the README tells a user: the module orientation follows from the
 * wiring, the matrix orientation from whether the board is mounted upside down. PAROLA and the other three wirings
 * are mirror images, which no orientation gives (OIL item 9). */
struct ModuleType {
    const char* Name;
    const Max7219WiringType* Wiring;
    bool UpsideDown;
    MaxMatrixMatrixOrientationType MatrixOrientation;
    MaxMatrixModuleOrientationType ModuleOrientation;
};

static const ModuleType Modules[] = {
    { "DR0CR0RR1", &Max7219Wirings[1], false, MAXMATRIX_MATRIX_ORIENTATION_0, MAXMATRIX_MODULE_ORIENTATION_0 },
    { "ICSTATION", &MAX7219_WIRING_ICSTATION, false, MAXMATRIX_MATRIX_ORIENTATION_0, MAXMATRIX_MODULE_ORIENTATION_90 },
    { "GENERIC", &MAX7219_WIRING_GENERIC, false, MAXMATRIX_MATRIX_ORIENTATION_0, MAXMATRIX_MODULE_ORIENTATION_180 },
    { "FC16", &MAX7219_WIRING_FC16, false, MAXMATRIX_MATRIX_ORIENTATION_0, MAXMATRIX_MODULE_ORIENTATION_270 },
    { "DR0CR0RR1_upside_down", &Max7219Wirings[1], true, MAXMATRIX_MATRIX_ORIENTATION_180, MAXMATRIX_MODULE_ORIENTATION_0 },
    { "ICSTATION_upside_down", &MAX7219_WIRING_ICSTATION, true, MAXMATRIX_MATRIX_ORIENTATION_180, MAXMATRIX_MODULE_ORIENTATION_90 },
    { "GENERIC_upside_down", &MAX7219_WIRING_GENERIC, true, MAXMATRIX_MATRIX_ORIENTATION_180, MAXMATRIX_MODULE_ORIENTATION_180 },
    { "FC16_upside_down", &MAX7219_WIRING_FC16, true, MAXMATRIX_MATRIX_ORIENTATION_180, MAXMATRIX_MODULE_ORIENTATION_270 },
};

class MaxMatrixModuleTest : public MaxMatrixTest, public ::testing::WithParamInterface<ModuleType> {};

static std::string moduleName(const ::testing::TestParamInfo<ModuleType>& Info)
{
    return Info.param.Name;
}

INSTANTIATE_TEST_SUITE_P(Modules, MaxMatrixModuleTest, ::testing::ValuesIn(Modules), moduleName);

/* a dot set at X, Y is lit at X, Y of the panel; the pattern has no symmetry a wrong mapping could hide behind */
TEST_P(MaxMatrixModuleTest, DotsAppearWhereTheyAreSet)
{
    const ModuleType& Module = GetParam();
    auto Pattern = [](uint16_t X, uint8_t Y) { return (X * X + 3 * Y + X * Y) % 3 == 0; };

    Matrix.setMatrixOrientation(Module.MatrixOrientation);
    Matrix.setModuleOrientation(Module.ModuleOrientation);
    for(byte X = 0; X < MAXMATRIX_NUMBER_OF_COLUMNS; X++) {
        for(byte Y = 0; Y < MAXMATRIX_ROW_NUMBER_OF_MODULE; Y++) Matrix.setDot(X, Y, Pattern(X, Y));
    }
    for(uint16_t X = 0; X < MAXMATRIX_NUMBER_OF_COLUMNS; X++) {
        for(uint8_t Y = 0; Y < MAXMATRIX_ROW_NUMBER_OF_MODULE; Y++) {
            EXPECT_EQ(Mock::Chain.isLit(*Module.Wiring, X, Y, Module.UpsideDown), Pattern(X, Y)) << "X " << X << ", Y " << Y;
        }
    }
}

TEST_F(MaxMatrixTest, InitConfiguresEveryChip)
{
    for(byte Module = 0; Module < MAXMATRIX_NUMBER_OF_MODULES; Module++) {
        SCOPED_TRACE(::testing::Message() << "Module " << static_cast<uint16_t>(Module));
        const Max7219& Chip = chipOfModule(Module);
        EXPECT_EQ(Chip.Shutdown, MAX7219_REG_SHUTDOWN_MODE_NORMAL_OPERATION);
        EXPECT_EQ(Chip.DisplayTest, MAX7219_REG_DISPLAY_TEST_NORMAL_OPERATION);
        EXPECT_EQ(Chip.ScanLimit, MAX7219_REG_SCAN_LIMIT_DISPLAY_DIGIT_0_TO_7);
        EXPECT_EQ(Chip.DecodeMode, MAX7219_REG_DECODE_MODE_NO_DECODE);
        EXPECT_EQ(Chip.Intensity, MAXMATRIX_INTENSITY_INIT_VALUE);
    }
}

TEST_F(MaxMatrixTest, IntensityReachesEveryChip)
{
    EXPECT_EQ(Matrix.setIntensity(MAX7219_REG_INTENSITY_MAX_VALUE).getError(), MAXMATRIX_E_OK);
    for(byte Module = 0; Module < MAXMATRIX_NUMBER_OF_MODULES; Module++) {
        EXPECT_EQ(chipOfModule(Module).Intensity, MAX7219_REG_INTENSITY_MAX_VALUE) << "Module " << static_cast<uint16_t>(Module);
    }
}

TEST_F(MaxMatrixTest, OutOfRangeChangesNothing)
{
    std::vector<byte> Before = bytes(Matrix.Panel.Wired.MatrixBuffer, sizeof(Matrix.Panel.Wired.MatrixBuffer));
    int32_t FramesBefore = Mock::Frames;

    EXPECT_EQ(Matrix.setIntensity(MAX7219_REG_INTENSITY_MAX_VALUE + 1).getError(), MAXMATRIX_E_OUT_OF_RANGE);
    EXPECT_EQ(bytes(Matrix.Panel.Wired.MatrixBuffer, sizeof(Matrix.Panel.Wired.MatrixBuffer)), Before);
    EXPECT_EQ(Mock::Frames, FramesBefore);
}

/* every way a call can fail, with the reason it gives: with stdReturnType a test stayed green when another check
 * than the one meant failed. A failing call changes nothing, draws nothing and starts no shift. "999x" is no number
 * rather than too large, although the digits run past the maximum before the letter comes. With
 * MAXMATRIX_DEV_ERROR_DETECT, the function of MaxMatrix that failed reports it once, under its own name. The command
 * reports nothing of its own, as a wrong line is no mistake of the sketch and its answer says what was wrong; only a
 * line that fails in a function of MaxMatrix, text= with a char the font lacks, is reported by that function. */
TEST_F(MaxMatrixTest, EveryErrorSaysWhy)
{
    const struct {
        const char* Name;
        MaxMatrixErrorType (*Call)(MaxMatrix&);
        MaxMatrixErrorType Expected;
        const char* Api;
    } Cases[] = {
        { "getColumn left", [](MaxMatrix& M) { return M.getColumn(-1).getError(); }, MAXMATRIX_E_OUT_OF_RANGE, "getColumn" },
        { "getColumn right", [](MaxMatrix& M) { return M.getColumn(MAXMATRIX_NUMBER_OF_COLUMNS).getError(); }, MAXMATRIX_E_OUT_OF_RANGE, "getColumn" },
        { "getColumn no module", [](MaxMatrix& M) { return M.getColumn(MAXMATRIX_NUMBER_OF_MODULES, 0).getError(); }, MAXMATRIX_E_OUT_OF_RANGE, "getColumn" },
        { "getColumn off its module", [](MaxMatrix& M) { return M.getColumn(0, MAXMATRIX_COLUMN_NUMBER_OF_MODULE).getError(); }, MAXMATRIX_E_OUT_OF_RANGE, "getColumn" },
        { "getRow above", [](MaxMatrix& M) { rowType Row; return M.getRow(-1, Row).getError(); }, MAXMATRIX_E_OUT_OF_RANGE, "getRow" },
        { "getRow below", [](MaxMatrix& M) { rowType Row; return M.getRow(MAXMATRIX_ROW_NUMBER_OF_MODULE, Row).getError(); }, MAXMATRIX_E_OUT_OF_RANGE, "getRow" },
        { "getRow no module", [](MaxMatrix& M) { return M.getRow(MAXMATRIX_NUMBER_OF_MODULES, 0).getError(); }, MAXMATRIX_E_OUT_OF_RANGE, "getRow" },
        { "getRow off its module", [](MaxMatrix& M) { return M.getRow(0, MAXMATRIX_ROW_NUMBER_OF_MODULE).getError(); }, MAXMATRIX_E_OUT_OF_RANGE, "getRow" },
        { "getDot left", [](MaxMatrix& M) { return M.getDot(-1, 0).getError(); }, MAXMATRIX_E_OUT_OF_RANGE, "getDot" },
        { "getDot right", [](MaxMatrix& M) { return M.getDot(MAXMATRIX_NUMBER_OF_COLUMNS, 0).getError(); }, MAXMATRIX_E_OUT_OF_RANGE, "getDot" },
        { "getDot above", [](MaxMatrix& M) { return M.getDot(0, -1).getError(); }, MAXMATRIX_E_OUT_OF_RANGE, "getDot" },
        { "getDot below", [](MaxMatrix& M) { return M.getDot(0, MAXMATRIX_ROW_NUMBER_OF_MODULE).getError(); }, MAXMATRIX_E_OUT_OF_RANGE, "getDot" },
        { "getSprite", [](MaxMatrix& M) { spriteType Sprite; return M.getSprite(255, &Sprite).getError(); }, MAXMATRIX_E_OUT_OF_RANGE, "getSprite" },
        { "setIntensity", [](MaxMatrix& M) { return M.setIntensity(MAX7219_REG_INTENSITY_MAX_VALUE + 1).getError(); }, MAXMATRIX_E_OUT_OF_RANGE, "setIntensity" },
        { "setChar", [](MaxMatrix& M) { return M.setChar(0, 0, '\x01').getError(); }, MAXMATRIX_E_UNKNOWN_CHAR, "setChar" },
        { "setText unknown char", [](MaxMatrix& M) { return M.setText("\x01").getError(); }, MAXMATRIX_E_UNKNOWN_CHAR, "setText" },
        { "setText NULL", [](MaxMatrix& M) { return M.setText(NULL).getError(); }, MAXMATRIX_E_NO_TEXT, "setText" },
        { "setCharWithShift", [](MaxMatrix& M) { return M.setCharWithShift('\x01').getError(); }, MAXMATRIX_E_UNKNOWN_CHAR, "setCharWithShift" },
        { "setTextWithShift NULL", [](MaxMatrix& M) { return M.setTextWithShift(NULL).getError(); }, MAXMATRIX_E_NO_TEXT, "setTextWithShift" },
        { "number empty", [](MaxMatrix&) { return MaxMatrixCommand::convertToNumber("", 255).getError(); }, MAXMATRIX_E_NOT_A_NUMBER, NULL },
        { "number with a letter", [](MaxMatrix&) { return MaxMatrixCommand::convertToNumber("12a", 255).getError(); }, MAXMATRIX_E_NOT_A_NUMBER, NULL },
        { "number too large", [](MaxMatrix&) { return MaxMatrixCommand::convertToNumber("256", 255).getError(); }, MAXMATRIX_E_OUT_OF_RANGE, NULL },
        { "number too large, then a letter", [](MaxMatrix&) { return MaxMatrixCommand::convertToNumber("999x", 255).getError(); }, MAXMATRIX_E_NOT_A_NUMBER, NULL },
        { "unknown command", [](MaxMatrix& M) { return feedText(M, "blink\n"); }, MAXMATRIX_E_UNKNOWN_COMMAND, NULL },
        { "line too long", [](MaxMatrix& M) { return feedText(M, "text=" + std::string(MAXMATRIX_COMMAND_LINE_LENGTH, 'x') + "\n"); }, MAXMATRIX_E_LINE_TOO_LONG, NULL },
    };

    for(const auto& Case : Cases) {
        Matrix.clear();
        int32_t FramesBefore = Mock::Frames;
        byte IntensityBefore = chipOfModule(0).Intensity;
        Reports.clear();
        EXPECT_EQ(Case.Call(Matrix), Case.Expected) << Case.Name;
        EXPECT_EQ(shownPicture(Matrix), picture([](int16_t, int16_t) { return false; })) << Case.Name;
        EXPECT_EQ(Mock::Frames, FramesBefore) << Case.Name;
        EXPECT_EQ(chipOfModule(0).Intensity, IntensityBefore) << Case.Name;
        EXPECT_EQ(Matrix.getState(), MAXMATRIX_STATE_READY) << Case.Name;
        std::vector<ReportType> Expected;
        if(DevErrorDetect && Case.Api != NULL) Expected.push_back(ReportType(Case.Api, Case.Expected));
        EXPECT_EQ(Reports, Expected) << Case.Name;
    }
}

/* every error has its text, as SerialControl answers with it, and a byte off the enum gets one as well; the texts
 * lie in a table of their own, so one moved by a place would give every error after it the text of the next */
TEST(MaxMatrixErrorTest, EveryErrorHasItsText)
{
    const char* const Texts[] = {
        "ok", "out of range", "unknown char", "no text", "not a number", "unknown command", "line too long"
    };
    ASSERT_EQ(sizeof(Texts) / sizeof(Texts[0]), NumberOfErrors);

    for(uint8_t Error = 0; Error < NumberOfErrors; Error++) {
        EXPECT_EQ(text(MaxMatrixStatus(static_cast<MaxMatrixErrorType>(Error)).getErrorText()), Texts[Error]) << ErrorNames[Error];
    }
    EXPECT_EQ(text(MaxMatrixResult<byte>(MAXMATRIX_E_UNKNOWN_CHAR).getErrorText()), "unknown char");
    /* a new error that ErrorNames lacks has a text of its own here, and fails */
    EXPECT_EQ(text(MaxMatrixStatus(static_cast<MaxMatrixErrorType>(NumberOfErrors)).getErrorText()), "unknown error");
}

#if(MAXMATRIX_DEV_ERROR_DETECT == STD_ON)
/* the default report prints what the tests record instead, so the printing is a function of its own and tested here */
TEST(MaxMatrixErrorTest, PrintedReportNamesFunctionAndError)
{
    Mock::SerialOutput.clear();
    maxMatrixPrintError(F("setText"), MAXMATRIX_E_UNKNOWN_CHAR);
    EXPECT_EQ(Mock::SerialOutput, "MaxMatrix setText: unknown char\r\n");
}
#endif

/* drawing off the panel leaves the panel as it was and sends nothing; column 8 of module 0 used to be checked
 * against the panel alone, which would have drawn it on module 1 */
TEST_F(MaxMatrixTest, DrawingOffThePanelIsLeftOut)
{
    rowType Row = { 0xFF, 0xFF, 0xFF, 0xFF };
    const struct {
        const char* Name;
        void (*Draw)(MaxMatrix&, const rowType);
    } Cases[] = {
        { "setDot left", [](MaxMatrix& M, const rowType) { M.setDot(-1, 0, true); } },
        { "setDot right", [](MaxMatrix& M, const rowType) { M.setDot(MAXMATRIX_NUMBER_OF_COLUMNS, 0, true); } },
        { "setDot above", [](MaxMatrix& M, const rowType) { M.setDot(0, -1, true); } },
        { "setDot below", [](MaxMatrix& M, const rowType) { M.setDot(0, MAXMATRIX_ROW_NUMBER_OF_MODULE, true); } },
        { "setColumn left", [](MaxMatrix& M, const rowType) { M.setColumn(-1, 0xFF); } },
        { "setColumn right", [](MaxMatrix& M, const rowType) { M.setColumn(MAXMATRIX_NUMBER_OF_COLUMNS, 0xFF); } },
        { "setColumn no module", [](MaxMatrix& M, const rowType) { M.setColumn(MAXMATRIX_NUMBER_OF_MODULES, 0, 0xFF); } },
        { "setColumn off its module", [](MaxMatrix& M, const rowType) { M.setColumn(0, MAXMATRIX_COLUMN_NUMBER_OF_MODULE, 0xFF); } },
        { "setRow above", [](MaxMatrix& M, const rowType R) { M.setRow(-1, R); } },
        { "setRow below", [](MaxMatrix& M, const rowType R) { M.setRow(MAXMATRIX_ROW_NUMBER_OF_MODULE, R); } },
        { "setRow no module", [](MaxMatrix& M, const rowType) { M.setRow(MAXMATRIX_NUMBER_OF_MODULES, 0, 0xFF); } },
        { "setRow off its module", [](MaxMatrix& M, const rowType) { M.setRow(0, MAXMATRIX_ROW_NUMBER_OF_MODULE, 0xFF); } },
    };

    for(const auto& Case : Cases) {
        Matrix.clear();
        int32_t FramesBefore = Mock::Frames;
        Case.Draw(Matrix, Row);
        EXPECT_EQ(shownPicture(Matrix), picture([](int16_t, int16_t) { return false; })) << Case.Name;
        EXPECT_EQ(Mock::Frames, FramesBefore) << Case.Name;
        EXPECT_TRUE(Reports.empty()) << Case.Name << " reported an error";
    }
}

/* a char partly off the panel shows the part on it, through either path of setSprite(): whole columns for a char
 * at the top, single dots for one moved down */
TEST_P(MaxMatrixOrientationTest, CharPartlyOffThePanelShowsThePartOnIt)
{
    spriteType Sprite;
    ASSERT_EQ(MaxMatrixFont::getCharSprite('A', &Sprite).getError(), MAXMATRIX_E_OK);
    const int16_t Width = Sprite[ASCII_TABLE_SPRITE_WIDTH], Height = Sprite[ASCII_TABLE_SPRITE_HEIGHT];
    const struct {
        int16_t X, Y;
    } Places[] = { { -2, 0 }, { MAXMATRIX_NUMBER_OF_COLUMNS - 2, 0 }, { -2, -3 }, { MAXMATRIX_NUMBER_OF_COLUMNS - 2, 5 } };

    for(const auto& Place : Places) {
        Matrix.clear();
        ASSERT_EQ(Matrix.setChar(Place.X, Place.Y, 'A').getError(), MAXMATRIX_E_OK);
        EXPECT_EQ(shownPicture(Matrix), picture([&Sprite, &Place, Width, Height](int16_t X, int16_t Y) {
            int16_t SpriteX = X - Place.X, SpriteY = Y - Place.Y;
            return SpriteX >= 0 && SpriteX < Width && SpriteY >= 0 && SpriteY < Height &&
                   bitRead(Sprite[ASCII_TABLE_SPRITE_COLUMN1 + SpriteX], SpriteY);
        })) << "X " << Place.X << ", Y " << Place.Y;
    }
}

/* a changed column of a module goes out in one frame with the same column of every other module */
TEST_F(MaxMatrixTest, ChangedColumnIsSentForEveryModule)
{
    const byte Module = MAXMATRIX_NUMBER_OF_MODULES - 2;
    int32_t FramesBefore = Mock::Frames;

    Matrix.Panel.Wired.storeBit(Module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE + 3, 1, true);
    Matrix.update();
    EXPECT_EQ(Mock::Frames - FramesBefore, 1);
    ASSERT_EQ(Mock::LastFrame.size(), 2u * MAXMATRIX_NUMBER_OF_MODULES);
    for(byte i = 0; i < MAXMATRIX_NUMBER_OF_MODULES; i++) {
        SCOPED_TRACE(::testing::Message() << "Module " << static_cast<uint16_t>(i));
        EXPECT_EQ(Mock::LastFrame[2 * i], MAX7219_REG_DIGIT3_ADDRESS);
        EXPECT_EQ(Mock::LastFrame[2 * i + 1], i == Module ? 0x02 : 0x00);
    }
}

/* clear() has to reach LEDs the buffer knows nothing of: after power-up the registers hold anything while the buffer
 * is empty, so a clear that sent only what changed in the buffer would send nothing */
TEST_F(MaxMatrixTest, InitClearsWhatThePowerUpLeft)
{
    Mock::reset(ChipSelectPin, MAXMATRIX_NUMBER_OF_MODULES);
    for(Max7219& Chip : Mock::Chain.Chips) {
        for(byte Digit = 0; Digit < Max7219::NumberOfDigits; Digit++) Chip.Digit[Digit] = 0xA5;
    }
    Matrix.init();
    EXPECT_EQ(display(), std::vector<byte>(MAXMATRIX_NUMBER_OF_COLUMNS, 0));
}

TEST_F(MaxMatrixTest, DrawingWhatIsShownSendsNothing)
{
    Matrix.setText("Hi!");
    int32_t FramesBefore = Mock::Frames;

    Matrix.setText("Hi!");
    Matrix.setDot(0, 0, Matrix.Panel.Wired.getColumn(0) & 1);
    EXPECT_EQ(Mock::Frames, FramesBefore);
}

/* the same drawing in both modes: a frame for every call that changed something, or none until update(), which sends
 * each changed column of a module once */
TEST_F(MaxMatrixTest, DrawingInBothModes)
{
    std::vector<byte> Drawn;
    int32_t FramesBefore = Mock::Frames;

    for(byte Column = 0; Column < MAXMATRIX_NUMBER_OF_COLUMNS; Column++) Matrix.setDot(Column, Column % MAXMATRIX_ROW_NUMBER_OF_MODULE, true);
    EXPECT_EQ(Mock::Frames - FramesBefore, MAXMATRIX_NUMBER_OF_COLUMNS);
    Drawn = display();

    Matrix.clear();
    Matrix.setAutoUpdate(false);
    FramesBefore = Mock::Frames;
    for(byte Column = 0; Column < MAXMATRIX_NUMBER_OF_COLUMNS; Column++) Matrix.setDot(Column, Column % MAXMATRIX_ROW_NUMBER_OF_MODULE, true);
    EXPECT_EQ(Mock::Frames, FramesBefore);
    EXPECT_EQ(display(), std::vector<byte>(MAXMATRIX_NUMBER_OF_COLUMNS, 0)) << "shown before update()";

    Matrix.update();
    EXPECT_EQ(Mock::Frames - FramesBefore, MAXMATRIX_COLUMN_NUMBER_OF_MODULE);
    EXPECT_EQ(display(), Drawn);
}

TEST_F(MaxMatrixTest, AutoUpdateOnSendsWhatWasDrawn)
{
    Matrix.setAutoUpdate(false);
    Matrix.setText("Hi!");
    EXPECT_EQ(display(), std::vector<byte>(MAXMATRIX_NUMBER_OF_COLUMNS, 0));

    Matrix.setAutoUpdate(true);
    EXPECT_NE(display(), std::vector<byte>(MAXMATRIX_NUMBER_OF_COLUMNS, 0));
}

TEST_F(MaxMatrixTest, BatchSendsAtTheEndOfTheBlock)
{
    int32_t FramesBefore = Mock::Frames;

    {
        MaxMatrixBatch Batch(Matrix);
        for(byte Column = 0; Column < MAXMATRIX_NUMBER_OF_COLUMNS; Column++) Matrix.setDot(Column, Column % MAXMATRIX_ROW_NUMBER_OF_MODULE, true);
        EXPECT_FALSE(Matrix.getAutoUpdate());
        EXPECT_EQ(Mock::Frames, FramesBefore);
        EXPECT_EQ(display(), std::vector<byte>(MAXMATRIX_NUMBER_OF_COLUMNS, 0)) << "shown inside the block";
    }
    EXPECT_TRUE(Matrix.getAutoUpdate());
    EXPECT_EQ(Mock::Frames - FramesBefore, MAXMATRIX_COLUMN_NUMBER_OF_MODULE);
}

/* the inner batch puts back the state it found, which the outer one had turned off */
TEST_F(MaxMatrixTest, NestedBatchesSendAtTheOutermost)
{
    int32_t FramesBefore = Mock::Frames;

    {
        MaxMatrixBatch Outer(Matrix);
        {
            MaxMatrixBatch Inner(Matrix);
            Matrix.setDot(1, 1, true);
        }
        EXPECT_EQ(Mock::Frames, FramesBefore) << "sent at the end of the inner block";
        Matrix.setDot(2, 2, true);
    }
    EXPECT_GT(Mock::Frames, FramesBefore);
}

/* a batch inside a sketch that sends with update() itself leaves it that way */
TEST_F(MaxMatrixTest, BatchKeepsAutoUpdateOff)
{
    Matrix.setAutoUpdate(false);
    int32_t FramesBefore = Mock::Frames;

    {
        MaxMatrixBatch Batch(Matrix);
        Matrix.setDot(1, 1, true);
    }
    EXPECT_FALSE(Matrix.getAutoUpdate());
    EXPECT_EQ(Mock::Frames, FramesBefore);
    Matrix.update();
}

TEST_F(MaxMatrixTest, ShiftIntervalStartsAtTheConfiguredValue)
{
    MaxMatrix Fresh(DataInPin, ChipSelectPin, ClockPin);

    EXPECT_EQ(Fresh.getShiftInterval(), MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE);
}

/* The tests below count the steps of a shifting char in SpriteShiftCounter, which charShiftTask() moves on by one per
 * step, and hold the clock, so that a step 50 ms on does not depend on how fast the host is. */

/* the first step comes at once, every further one an interval after the step before */
TEST_F(MaxMatrixTest, ShiftWaitsForTheInterval)
{
    Mock::setMillis(1000);
    Matrix.setShiftInterval(50);
    ASSERT_EQ(Matrix.setCharWithShift('#').getError(), MAXMATRIX_E_OK);
    const int16_t Start = Matrix.Text.SpriteShiftCounter;

    Matrix.shiftTask();
    EXPECT_EQ(Matrix.Text.SpriteShiftCounter, Start + 1) << "no first step at once";
    Mock::setMillis(1049);
    Matrix.shiftTask();
    EXPECT_EQ(Matrix.Text.SpriteShiftCounter, Start + 1) << "a step before the interval had passed";
    Mock::setMillis(1050);
    Matrix.shiftTask();
    EXPECT_EQ(Matrix.Text.SpriteShiftCounter, Start + 2);
}

/* a call after loop() was held up takes one step, not the ones it missed, and the interval counts from there */
TEST_F(MaxMatrixTest, LateShiftDoesNotCatchUp)
{
    Mock::setMillis(1000);
    Matrix.setShiftInterval(50);
    ASSERT_EQ(Matrix.setCharWithShift('#').getError(), MAXMATRIX_E_OK);
    const int16_t Start = Matrix.Text.SpriteShiftCounter;

    Matrix.shiftTask();
    Mock::setMillis(1180);
    Matrix.shiftTask();
    Matrix.shiftTask();
    EXPECT_EQ(Matrix.Text.SpriteShiftCounter, Start + 2);
    Mock::setMillis(1229);
    Matrix.shiftTask();
    EXPECT_EQ(Matrix.Text.SpriteShiftCounter, Start + 2);
    Mock::setMillis(1230);
    Matrix.shiftTask();
    EXPECT_EQ(Matrix.Text.SpriteShiftCounter, Start + 3);
}

TEST_F(MaxMatrixTest, ShiftIntervalZeroStepsOnEveryCall)
{
    Mock::setMillis(1000);
    Matrix.setShiftInterval(0);
    ASSERT_EQ(Matrix.setCharWithShift('#').getError(), MAXMATRIX_E_OK);
    const int16_t Start = Matrix.Text.SpriteShiftCounter;

    for(uint8_t Call = 0; Call < 3; Call++) Matrix.shiftTask();
    EXPECT_EQ(Matrix.Text.SpriteShiftCounter, Start + 3);
}

/* a new char or text starts at once, however recently the one before took a step */
TEST_F(MaxMatrixTest, NewShiftStartsAtOnce)
{
    Mock::setMillis(1000);
    Matrix.setShiftInterval(50);
    ASSERT_EQ(Matrix.setCharWithShift('#').getError(), MAXMATRIX_E_OK);
    Matrix.shiftTask();

    Mock::setMillis(1010);
    ASSERT_EQ(Matrix.setCharWithShift('+').getError(), MAXMATRIX_E_OK);
    const int16_t Start = Matrix.Text.SpriteShiftCounter;
    Matrix.shiftTask();
    EXPECT_EQ(Matrix.Text.SpriteShiftCounter, Start + 1);
}

/* chip select was not an output in SPI mode, and the SPI transaction was never ended */
TEST_F(MaxMatrixTest, TransfersAreFramed)
{
    char Text[] = "Hello";

    EXPECT_EQ(Mock::PinMode[ChipSelectPin], OUTPUT);
    Matrix.setText("Hi!");
    Matrix.setModuleOrientation(MAXMATRIX_MODULE_ORIENTATION_90);
    shiftWholeText(Text);

    EXPECT_GT(Mock::Frames, 0);
    EXPECT_EQ(Mock::Errors, 0);
    EXPECT_EQ(Mock::PinLevel[ChipSelectPin], HIGH);
    EXPECT_FALSE(Mock::TransactionOpen);
    EXPECT_EQ(Mock::usesSpi(), MAXMATRIX_USE_SPI == STD_ON);
}

TEST_F(MaxMatrixTest, TextShiftFinishes)
{
    char Text[] = "Hello World!";

    shiftWholeText(Text);
    EXPECT_EQ(Matrix.getState(), MAXMATRIX_STATE_READY);
}

/* the Arduino IDE saves sketches as UTF-8, so an umlaut arrives as two bytes */
TEST_F(MaxMatrixTest, UmlautsFromUtf8AndLatin1Match)
{
    std::vector<byte> Utf8, Latin1;

    Matrix.clear();
    EXPECT_EQ(Matrix.setText("Äöü!").getError(), MAXMATRIX_E_OK);
    Utf8 = bytes(Matrix.Panel.Wired.MatrixBuffer, sizeof(Matrix.Panel.Wired.MatrixBuffer));
    Matrix.clear();
    EXPECT_EQ(Matrix.setText("\xC4\xF6\xFC!").getError(), MAXMATRIX_E_OK);
    Latin1 = bytes(Matrix.Panel.Wired.MatrixBuffer, sizeof(Matrix.Panel.Wired.MatrixBuffer));
    EXPECT_EQ(Utf8, Latin1);

    char ShiftUtf8[] = "Grüße Ä";
    char ShiftLatin1[] = "Gr\xFC\xDF" "e \xC4";
    shiftWholeText(ShiftUtf8);
    EXPECT_EQ(Matrix.getState(), MAXMATRIX_STATE_READY);
    Utf8 = bytes(Matrix.Panel.Wired.MatrixBuffer, sizeof(Matrix.Panel.Wired.MatrixBuffer));
    shiftWholeText(ShiftLatin1);
    Latin1 = bytes(Matrix.Panel.Wired.MatrixBuffer, sizeof(Matrix.Panel.Wired.MatrixBuffer));
    EXPECT_EQ(Utf8, Latin1);
}

TEST_F(MaxMatrixTest, EveryUmlautHasItsOwnSprite)
{
    const char* Umlauts[] = { "Ä", "Ö", "Ü", "ä", "ö", "ü" };
    char Char;

    for(byte i = 0; i < 6; i++) {
        SCOPED_TRACE(Umlauts[i]);
        EXPECT_EQ(MaxMatrixFont::decodeChar(Umlauts[i], &Char), 2);
        MaxMatrixResult<spriteIndexType> SpriteIndex = MaxMatrixFont::convertCharToSprite(Char);
        EXPECT_EQ(SpriteIndex.getError(), MAXMATRIX_E_OK);
        EXPECT_EQ(SpriteIndex.getValue(), 95 + i);
    }

    /* the Ü sprite used to be a copy of the O */
    spriteType Ue, O;
    EXPECT_EQ(Matrix.getSprite(97, &Ue).getError(), MAXMATRIX_E_OK);
    EXPECT_EQ(Matrix.getSprite('O' + MAXMATRIX_ASCII_TABLE_OFFSET, &O).getError(), MAXMATRIX_E_OK);
    EXPECT_NE(bytes(Ue, sizeof(Ue)), bytes(O, sizeof(O)));
}

TEST_F(MaxMatrixTest, DecodeStopsAtEndOfString)
{
    char Char;

    EXPECT_EQ(MaxMatrixFont::decodeChar("A", &Char), 1);
    EXPECT_EQ(Char, 'A');
    EXPECT_EQ(MaxMatrixFont::decodeChar("\xC3", &Char), 1);

    char Truncated[] = "a\xC3";
    shiftWholeText(Truncated);
    EXPECT_EQ(Matrix.getState(), MAXMATRIX_STATE_READY);
}

/* an unknown char used to be shifted in all the same, with whatever sprite was left over; that the
 * shift finishes does not show it, so what went to the chain is compared with the text without it.
 * Skipping it then took a shiftTask() call of its own that sent nothing, and the text stood still for
 * a step, so the calls are compared as well - in the middle of the text and at its end. */
TEST_F(MaxMatrixTest, UnknownChars)
{
    EXPECT_EQ(Matrix.setText("a\x01").getError(), MAXMATRIX_E_UNKNOWN_CHAR);

    char Known[] = "ab";
    int32_t Frames = shiftWholeText(Known);
    uint16_t Calls = ShiftCalls;

    char InTheMiddle[] = "a\x01\x02" "b";
    EXPECT_EQ(shiftWholeText(InTheMiddle), Frames);
    EXPECT_EQ(ShiftCalls, Calls);
    EXPECT_EQ(Matrix.getState(), MAXMATRIX_STATE_READY);

    char AtTheEnd[] = "ab\x01";
    EXPECT_EQ(shiftWholeText(AtTheEnd), Frames);
    EXPECT_EQ(ShiftCalls, Calls);
    EXPECT_EQ(Matrix.getState(), MAXMATRIX_STATE_READY);
}

class MaxMatrixCommandTest : public MaxMatrixTest {
  protected:
    MaxMatrixCommand Command;

    /* feeds a line char by char, as Serial would; every char but the last has to leave the line pending */
    MaxMatrixResult<MaxMatrixCommandStateType> feedLine(const std::string& Text)
    {
        for(size_t i = 0; i + 1 < Text.size(); i++) {
            EXPECT_EQ(Command.feed(Matrix, Text[i]), MAXMATRIX_COMMAND_PENDING) << "char " << i << " of " << Text;
        }
        return Command.feed(Matrix, Text.back());
    }

    /* the display after every step of the shift running now, until it is done */
    std::vector<std::vector<byte>> shiftSteps()
    {
        std::vector<std::vector<byte>> Steps;
        for(uint16_t Step = 0; Step < 1000 && Matrix.getState() != MAXMATRIX_STATE_READY; Step++) {
            Matrix.shiftTask();
            Steps.push_back(display());
        }
        return Steps;
    }
};

TEST_F(MaxMatrixCommandTest, TextShowsWhatSetTextShows)
{
    Matrix.setText("xx");
    ASSERT_EQ(feedLine("text=Hi!\n"), MAXMATRIX_COMMAND_DONE);
    const std::vector<byte> Shown = display();

    Matrix.clear();
    Matrix.setText("Hi!");
    EXPECT_EQ(Shown, display());
}

TEST_F(MaxMatrixCommandTest, SettingsAndDotsReachTheMatrix)
{
    EXPECT_EQ(feedLine("intensity=8\n"), MAXMATRIX_COMMAND_DONE);
    EXPECT_EQ(chipOfModule(0).Intensity, 8);
    EXPECT_EQ(feedLine("speed=120\n"), MAXMATRIX_COMMAND_DONE);
    EXPECT_EQ(Matrix.getShiftInterval(), 120);
    EXPECT_EQ(feedLine("space=3\n"), MAXMATRIX_COMMAND_DONE);
    EXPECT_EQ(Matrix.getSpaceBetweenChars(), 3);

    EXPECT_EQ(feedLine("setDot=5,3,1\n"), MAXMATRIX_COMMAND_DONE);
    EXPECT_EQ(Matrix.getDot(5, 3).getError(), MAXMATRIX_E_OK);
    EXPECT_TRUE(Matrix.getDot(5, 3).getValue());
    EXPECT_EQ(feedLine("setDot=5,3,0\n"), MAXMATRIX_COMMAND_DONE);
    EXPECT_EQ(Matrix.getDot(5, 3).getError(), MAXMATRIX_E_OK);
    EXPECT_FALSE(Matrix.getDot(5, 3).getValue());

    Matrix.setText("Hi");
    EXPECT_EQ(feedLine("clear\n"), MAXMATRIX_COMMAND_DONE);
    EXPECT_EQ(display(), std::vector<byte>(MAXMATRIX_NUMBER_OF_COLUMNS, 0));
}

/* setTextWithShift() keeps only a pointer: a shift that read its text out of the line buffer would shift whatever
 * line came next */
TEST_F(MaxMatrixCommandTest, TextShiftKeepsItsTextWhileLinesFollow)
{
    char Text[] = "AB";

    ASSERT_EQ(feedLine("textShift=AB\n"), MAXMATRIX_COMMAND_DONE);
    ASSERT_EQ(feedLine("intensity=3\n"), MAXMATRIX_COMMAND_DONE);
    const std::vector<std::vector<byte>> Commanded = shiftSteps();

    Matrix.clear();
    ASSERT_EQ(Matrix.setTextWithShift(Text).getError(), MAXMATRIX_E_OK);
    EXPECT_EQ(Commanded, shiftSteps());
}

TEST_F(MaxMatrixCommandTest, WrongLinesAreRefused)
{
    const struct {
        const char* Line;
        MaxMatrixErrorType Error;
    } Cases[] = {
        { "blink\n", MAXMATRIX_E_UNKNOWN_COMMAND },
        { "clear=1\n", MAXMATRIX_E_UNKNOWN_COMMAND },
        { "intensity=16\n", MAXMATRIX_E_OUT_OF_RANGE },
        { "intensity=\n", MAXMATRIX_E_NOT_A_NUMBER },
        { "intensity=x\n", MAXMATRIX_E_NOT_A_NUMBER },
        { "speed=65536\n", MAXMATRIX_E_OUT_OF_RANGE },
        { "speed=99999999999999999999\n", MAXMATRIX_E_OUT_OF_RANGE },
        /* 2^32, which a count in 32 bits wraps round to 0, a speed it would take */
        { "speed=4294967296\n", MAXMATRIX_E_OUT_OF_RANGE },
        { "space=-1\n", MAXMATRIX_E_NOT_A_NUMBER },
        { "setDot=1,2\n", MAXMATRIX_E_NOT_A_NUMBER },
        { "setDot=32,0,1\n", MAXMATRIX_E_OUT_OF_RANGE },
        { "setDot=1,8,1\n", MAXMATRIX_E_OUT_OF_RANGE },
        { "setDot=1,2,2\n", MAXMATRIX_E_OUT_OF_RANGE },
        { "textShift=\n", MAXMATRIX_E_NO_TEXT },
        { "text=a\x01\n", MAXMATRIX_E_UNKNOWN_CHAR },
    };

    for(const auto& Case : Cases) EXPECT_EQ(feedLine(Case.Line), Case.Error) << Case.Line;
    EXPECT_EQ(Matrix.getShiftInterval(), 0) << "changed by a refused speed=";
}

TEST_F(MaxMatrixCommandTest, LineEndsInNewlineOrCarriageReturnNewline)
{
    EXPECT_EQ(feedLine("intensity=4\r\n"), MAXMATRIX_COMMAND_DONE);
    EXPECT_EQ(chipOfModule(0).Intensity, 4);
    EXPECT_EQ(feedLine("\n"), MAXMATRIX_COMMAND_PENDING) << "an empty line";
    EXPECT_EQ(feedLine("intensity=5\n"), MAXMATRIX_COMMAND_DONE);
}

/* a line cut short would carry out a different command, so a line too long is dropped whole; the next one is read */
TEST_F(MaxMatrixCommandTest, LineTooLongIsDroppedWhole)
{
    const std::string Long = "text=" + std::string(MAXMATRIX_COMMAND_LINE_LENGTH, 'x') + "\n";

    EXPECT_EQ(feedLine(Long), MAXMATRIX_E_LINE_TOO_LONG);
    EXPECT_EQ(display(), std::vector<byte>(MAXMATRIX_NUMBER_OF_COLUMNS, 0));
    EXPECT_EQ(feedLine("intensity=2\n"), MAXMATRIX_COMMAND_DONE);
    EXPECT_EQ(chipOfModule(0).Intensity, 2);
}

TEST_F(MaxMatrixCommandTest, LineOfFullLengthIsTaken)
{
    const std::string Text = "text=" + std::string(MAXMATRIX_COMMAND_LINE_LENGTH - 5, 'x') + "\n";

    EXPECT_EQ(feedLine(Text), MAXMATRIX_COMMAND_DONE);
}
