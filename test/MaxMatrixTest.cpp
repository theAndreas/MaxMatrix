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

/* The low level functions and the matrix buffer are private, and some of what is tested here has no
 * public path: setColumnOnAllModulesLL is only reached through clear(), which zeroes the buffer
 * afterwards and so hides a wrong index. Included after GoogleTest and the standard headers, which must
 * not see it - defined above them, it rewrites their classes too. */
#define private public
#include "MaxMatrix.h"
#undef private

static const byte DataInPin = 11;
static const byte ChipSelectPin = 10;
static const byte ClockPin = 13;

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
        Matrix.init();
    }

    /* whatever a test did, the LEDs have to show what the buffer says - the get functions read the buffer, because
     * the MAX7219 cannot be read back, so a buffer that drifts from the display makes them lie */
    void TearDown() override
    {
        EXPECT_EQ(display(), bytes(Matrix.MatrixBuffer, sizeof(Matrix.MatrixBuffer))) << "display differs from MatrixBuffer";
    }

    /* the shiftTask() calls the last shiftWholeText() took, each one a step of the text on the panel */
    int ShiftCalls = 0;

    /* returns the number of frames the shift sent to the chain */
    int shiftWholeText(char* Text)
    {
        Matrix.clear();
        int FramesBefore = Mock::Frames;
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
        SCOPED_TRACE(::testing::Message() << "Row " << int(Row));
        EXPECT_EQ(Matrix.setRow(Row, In), E_OK);
        EXPECT_EQ(Matrix.getRow(Row, Out), E_OK);
        EXPECT_EQ(bytes(Out, sizeof(Out)), bytes(In, sizeof(In)));
    }
}

/* the display has to follow the buffer during the shift too, not only once the text has left it */
TEST_P(MaxMatrixOrientationTest, DisplayFollowsTheShift)
{
    char Text[] = "Hi \xC4!";

    Matrix.setTextWithShift(Text);
    for(int Step = 0; Step < 1000 && Matrix.getState() != MAXMATRIX_STATE_READY; Step++) {
        Matrix.shiftTask();
        ASSERT_EQ(display(), bytes(Matrix.MatrixBuffer, sizeof(Matrix.MatrixBuffer))) << "after step " << Step;
    }
}

static bool pattern(int X, int Y)
{
    return (X * X + 3 * Y + X * Y) % 3 == 0;
}

/* a picture as rows of '#' and '.', so that a failure prints both pictures */
template<typename DotType>
static std::vector<std::string> picture(DotType Dot)
{
    std::vector<std::string> Rows;
    for(int Y = 0; Y < MAXMATRIX_ROW_NUMBER_OF_MODULE; Y++) {
        std::string Row;
        for(int X = 0; X < MAXMATRIX_NUMBER_OF_COLUMNS; X++) Row += Dot(X, Y) ? '#' : '.';
        Rows.push_back(Row);
    }
    return Rows;
}

/* the picture as the library reports it, through getDot */
static std::vector<std::string> shownPicture(MaxMatrix& Matrix)
{
    return picture([&Matrix](int X, int Y) { bool Value = false; Matrix.getDot(X, Y, &Value); return Value; });
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

    ASSERT_EQ(Matrix.setRow(2, Row), E_OK);
    EXPECT_EQ(shownPicture(Matrix), picture([&Row](int X, int Y) {
        return Y == 2 && bitRead(Row[X / MAXMATRIX_COLUMN_NUMBER_OF_MODULE], 7 - X % MAXMATRIX_COLUMN_NUMBER_OF_MODULE);
    }));

    Matrix.clear();
    ASSERT_EQ(Matrix.setColumn(5, 0xA3), E_OK);
    EXPECT_EQ(shownPicture(Matrix), picture([](int X, int Y) { return X == 5 && bitRead(0xA3, Y); }));
    for(byte Y = 0; Y < MAXMATRIX_ROW_NUMBER_OF_MODULE; Y++) {
        byte Value = 0;
        EXPECT_EQ(Matrix.getRow(0, Y, &Value), E_OK);
        EXPECT_EQ(Value, bitRead(0xA3, Y) ? 1 << (7 - 5) : 0) << "Row " << int(Y);
    }
}

/* every shift moves the whole picture by one dot as the viewer sees it, across the modules */
TEST_P(MaxMatrixOrientationTest, ShiftsMoveThePicture)
{
    const struct {
        const char* Name;
        void (*Shift)(MaxMatrix&);
        int SourceX, SourceY;
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
    const int Width = MAXMATRIX_NUMBER_OF_COLUMNS, Height = MAXMATRIX_ROW_NUMBER_OF_MODULE;

    for(const auto& Case : Cases) {
        Matrix.clear();
        drawPattern(Matrix);
        Case.Shift(Matrix);
        EXPECT_EQ(shownPicture(Matrix), picture([&Case, Width, Height](int X, int Y) {
            int SourceX = X + Case.SourceX, SourceY = Y + Case.SourceY;
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
            SCOPED_TRACE(::testing::Message() << "Column " << int(Column) << ", Row " << int(Row));
            bool Expected = (Column * 3 + Row) & 1;
            bool Value = !Expected;
            EXPECT_EQ(Matrix.setDot(Column, Row, Expected), E_OK);
            EXPECT_EQ(Matrix.getDot(Column, Row, &Value), E_OK);
            EXPECT_EQ(Value, Expected);
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
    auto Pattern = [](unsigned X, unsigned Y) { return (X * X + 3 * Y + X * Y) % 3 == 0; };

    Matrix.setMatrixOrientation(Module.MatrixOrientation);
    Matrix.setModuleOrientation(Module.ModuleOrientation);
    for(byte X = 0; X < MAXMATRIX_NUMBER_OF_COLUMNS; X++) {
        for(byte Y = 0; Y < MAXMATRIX_ROW_NUMBER_OF_MODULE; Y++) Matrix.setDot(X, Y, Pattern(X, Y));
    }
    for(unsigned X = 0; X < MAXMATRIX_NUMBER_OF_COLUMNS; X++) {
        for(unsigned Y = 0; Y < MAXMATRIX_ROW_NUMBER_OF_MODULE; Y++) {
            EXPECT_EQ(Mock::Chain.isLit(*Module.Wiring, X, Y, Module.UpsideDown), Pattern(X, Y)) << "X " << X << ", Y " << Y;
        }
    }
}

TEST_F(MaxMatrixTest, InitConfiguresEveryChip)
{
    for(byte Module = 0; Module < MAXMATRIX_NUMBER_OF_MODULES; Module++) {
        SCOPED_TRACE(::testing::Message() << "Module " << int(Module));
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
    EXPECT_EQ(Matrix.setIntensity(MAX7219_REG_INTENSITY_MAX_VALUE), E_OK);
    for(byte Module = 0; Module < MAXMATRIX_NUMBER_OF_MODULES; Module++) {
        EXPECT_EQ(chipOfModule(Module).Intensity, MAX7219_REG_INTENSITY_MAX_VALUE) << "Module " << int(Module);
    }
}

TEST_F(MaxMatrixTest, OutOfRangeChangesNothing)
{
    std::vector<byte> Before = bytes(Matrix.MatrixBuffer, sizeof(Matrix.MatrixBuffer));

    EXPECT_EQ(Matrix.setDot(MAXMATRIX_NUMBER_OF_COLUMNS, 0, true), E_NOT_OK);
    EXPECT_EQ(Matrix.setDot(0, MAXMATRIX_ROW_NUMBER_OF_MODULE, true), E_NOT_OK);
    EXPECT_EQ(Matrix.setColumn(MAXMATRIX_NUMBER_OF_COLUMNS, 0xFF), E_NOT_OK);
    EXPECT_EQ(Matrix.setRow(0, MAXMATRIX_ROW_NUMBER_OF_MODULE, 0xFF), E_NOT_OK);
    EXPECT_EQ(Matrix.setIntensity(MAX7219_REG_INTENSITY_MAX_VALUE + 1), E_NOT_OK);
    EXPECT_EQ(bytes(Matrix.MatrixBuffer, sizeof(Matrix.MatrixBuffer)), Before);
}

/* the buffer index was Column * Module, which clear() covers up by zeroing the buffer afterwards */
TEST_F(MaxMatrixTest, ColumnOnAllModules)
{
    EXPECT_EQ(Matrix.setColumnOnAllModulesLL(3, 0x42), E_OK);
    for(byte Module = 0; Module < MAXMATRIX_NUMBER_OF_MODULES; Module++) {
        for(byte Column = 0; Column < MAXMATRIX_COLUMN_NUMBER_OF_MODULE; Column++) {
            SCOPED_TRACE(::testing::Message() << "Module " << int(Module) << ", Column " << int(Column));
            EXPECT_EQ(Matrix.MatrixBuffer[Module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE + Column], Column == 3 ? 0x42 : 0);
        }
    }
}

/* one address/value pair per module, and every module but the addressed one gets a no-op */
TEST_F(MaxMatrixTest, ColumnFrameAddressesOneModule)
{
    const byte Module = MAXMATRIX_NUMBER_OF_MODULES - 2;

    EXPECT_EQ(Matrix.setColumnLL(Module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE + 3, 0x5A), E_OK);
    ASSERT_EQ(Mock::LastFrame.size(), 2u * MAXMATRIX_NUMBER_OF_MODULES);
    for(byte i = 0; i < MAXMATRIX_NUMBER_OF_MODULES; i++) {
        SCOPED_TRACE(::testing::Message() << "Module " << int(i));
        if(i == Module) {
            EXPECT_EQ(Mock::LastFrame[2 * i], MAX7219_REG_DIGIT3_ADDRESS);
            EXPECT_EQ(Mock::LastFrame[2 * i + 1], 0x5A);
        } else {
            EXPECT_EQ(Mock::LastFrame[2 * i], MAX7219_REG_NO_OP_ADDRESS);
        }
    }
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
    EXPECT_EQ(Matrix.setText("Äöü!"), E_OK);
    Utf8 = bytes(Matrix.MatrixBuffer, sizeof(Matrix.MatrixBuffer));
    Matrix.clear();
    EXPECT_EQ(Matrix.setText("\xC4\xF6\xFC!"), E_OK);
    Latin1 = bytes(Matrix.MatrixBuffer, sizeof(Matrix.MatrixBuffer));
    EXPECT_EQ(Utf8, Latin1);

    char ShiftUtf8[] = "Grüße Ä";
    char ShiftLatin1[] = "Gr\xFC\xDF" "e \xC4";
    shiftWholeText(ShiftUtf8);
    EXPECT_EQ(Matrix.getState(), MAXMATRIX_STATE_READY);
    Utf8 = bytes(Matrix.MatrixBuffer, sizeof(Matrix.MatrixBuffer));
    shiftWholeText(ShiftLatin1);
    Latin1 = bytes(Matrix.MatrixBuffer, sizeof(Matrix.MatrixBuffer));
    EXPECT_EQ(Utf8, Latin1);
}

TEST_F(MaxMatrixTest, EveryUmlautHasItsOwnSprite)
{
    const char* Umlauts[] = { "Ä", "Ö", "Ü", "ä", "ö", "ü" };
    char Char;
    spriteIndexType SpriteIndex;

    for(byte i = 0; i < 6; i++) {
        SCOPED_TRACE(Umlauts[i]);
        EXPECT_EQ(Matrix.decodeChar(Umlauts[i], &Char), 2);
        EXPECT_EQ(Matrix.convertCharToSprite(Char, &SpriteIndex), E_OK);
        EXPECT_EQ(SpriteIndex, 95 + i);
    }

    /* the Ü sprite used to be a copy of the O */
    spriteType Ue, O;
    EXPECT_EQ(Matrix.getSprite(97, &Ue), E_OK);
    EXPECT_EQ(Matrix.getSprite('O' + MAXMATRIX_ASCII_TABLE_OFFSET, &O), E_OK);
    EXPECT_NE(bytes(Ue, sizeof(Ue)), bytes(O, sizeof(O)));
}

TEST_F(MaxMatrixTest, DecodeStopsAtEndOfString)
{
    char Char;

    EXPECT_EQ(Matrix.decodeChar("A", &Char), 1);
    EXPECT_EQ(Char, 'A');
    EXPECT_EQ(Matrix.decodeChar("\xC3", &Char), 1);

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
    EXPECT_EQ(Matrix.setText("a\x01"), E_NOT_OK);

    char Known[] = "ab";
    int Frames = shiftWholeText(Known);
    int Calls = ShiftCalls;

    char InTheMiddle[] = "a\x01\x02" "b";
    EXPECT_EQ(shiftWholeText(InTheMiddle), Frames);
    EXPECT_EQ(ShiftCalls, Calls);
    EXPECT_EQ(Matrix.getState(), MAXMATRIX_STATE_READY);

    char AtTheEnd[] = "ab\x01";
    EXPECT_EQ(shiftWholeText(AtTheEnd), Frames);
    EXPECT_EQ(ShiftCalls, Calls);
    EXPECT_EQ(Matrix.getState(), MAXMATRIX_STATE_READY);
}
