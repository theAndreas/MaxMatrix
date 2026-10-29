/* Host-side tests of MaxMatrix against the Arduino stand-in in mock/.
 *
 * Built twice by the Makefile, once with bit banging and once with MAXMATRIX_USE_SPI=STD_ON, because the
 * two transfer paths are separate code. What this cannot show is anything about timing or the electrical
 * side - whether a real MAX7219 accepts the frames is only known on hardware. */
#include <stdio.h>
#include "Arduino.h"

/* The low level functions and the matrix buffer are private, and some of what is tested here has no
 * public path: setColumnOnAllModulesLL is only reached through clear(), which zeroes the buffer
 * afterwards and so hides a wrong index. Included after the standard headers, which must not see it. */
#define private public
#include "MaxMatrix.h"
#undef private

static const byte DataInPin = 11;
static const byte ChipSelectPin = 10;
static const byte ClockPin = 13;

static int Failures = 0;

#define CHECK(Condition) \
    do { if(!(Condition)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #Condition); Failures++; } } while(0)

static const MaxMatrixModuleOrientationType Orientations[] = {
    MAXMATRIX_MODULE_ORIENTATION_0, MAXMATRIX_MODULE_ORIENTATION_90,
    MAXMATRIX_MODULE_ORIENTATION_180, MAXMATRIX_MODULE_ORIENTATION_270
};

static void initMatrix(MaxMatrix& Matrix)
{
    Mock::reset(ChipSelectPin);
    Matrix.init();
}

static void shiftWholeText(MaxMatrix& Matrix, char* Text)
{
    Matrix.clear();
    Matrix.setTextWithShift(Text);
    for(int i = 0; i < 1000 && Matrix.getState() != MAXMATRIX_STATE_READY; i++) Matrix.shiftTask();
}

/* the bit position used to run on across modules, so only module 0 got its row */
static void testRowRoundTripInEveryOrientation()
{
    MaxMatrix Matrix(DataInPin, ChipSelectPin, ClockPin);
    initMatrix(Matrix);
    rowType In, Out;
    for(byte Module = 0; Module < MAXMATRIX_NUMBER_OF_MODULES; Module++) In[Module] = 0xA5 ^ (Module * 0x3C);

    for(MaxMatrixModuleOrientationType Orientation : Orientations) {
        Matrix.clear();
        Matrix.setModuleOrientation(Orientation);
        for(byte Row = 0; Row < MAXMATRIX_ROW_NUMBER_OF_MODULE; Row++) {
            CHECK(Matrix.setRow(Row, In) == E_OK);
            CHECK(Matrix.getRow(Row, Out) == E_OK);
            CHECK(memcmp(In, Out, sizeof(In)) == 0);
        }
    }
}

static void testDotRoundTripInEveryOrientation()
{
    MaxMatrix Matrix(DataInPin, ChipSelectPin, ClockPin);
    initMatrix(Matrix);

    for(MaxMatrixModuleOrientationType Orientation : Orientations) {
        Matrix.clear();
        Matrix.setModuleOrientation(Orientation);
        for(byte Column = 0; Column < MAXMATRIX_NUMBER_OF_COLUMNS; Column++) {
            for(byte Row = 0; Row < MAXMATRIX_ROW_NUMBER_OF_MODULE; Row++) {
                bool Expected = (Column * 3 + Row) & 1;
                bool Value = !Expected;
                CHECK(Matrix.setDot(Column, Row, Expected) == E_OK);
                CHECK(Matrix.getDot(Column, Row, &Value) == E_OK);
                CHECK(Value == Expected);
            }
        }
    }
}

static void testOutOfRangeChangesNothing()
{
    MaxMatrix Matrix(DataInPin, ChipSelectPin, ClockPin);
    initMatrix(Matrix);
    byte Before[MAXMATRIX_BUFFER_SIZE];
    memcpy(Before, Matrix.MatrixBuffer, sizeof(Before));

    CHECK(Matrix.setDot(MAXMATRIX_NUMBER_OF_COLUMNS, 0, true) == E_NOT_OK);
    CHECK(Matrix.setDot(0, MAXMATRIX_ROW_NUMBER_OF_MODULE, true) == E_NOT_OK);
    CHECK(Matrix.setColumn(MAXMATRIX_NUMBER_OF_COLUMNS, 0xFF) == E_NOT_OK);
    CHECK(Matrix.setRow(0, MAXMATRIX_ROW_NUMBER_OF_MODULE, 0xFF) == E_NOT_OK);
    CHECK(Matrix.setIntensity(MAX7219_REG_INTENSITY_MAX_VALUE + 1) == E_NOT_OK);
    CHECK(memcmp(Before, Matrix.MatrixBuffer, sizeof(Before)) == 0);
}

/* the buffer index was Column * Module, which clear() covers up by zeroing the buffer afterwards */
static void testColumnOnAllModules()
{
    MaxMatrix Matrix(DataInPin, ChipSelectPin, ClockPin);
    initMatrix(Matrix);

    CHECK(Matrix.setColumnOnAllModulesLL(3, 0x42) == E_OK);
    for(byte Module = 0; Module < MAXMATRIX_NUMBER_OF_MODULES; Module++) {
        for(byte Column = 0; Column < MAXMATRIX_COLUMN_NUMBER_OF_MODULE; Column++) {
            CHECK(Matrix.MatrixBuffer[Module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE + Column] == (Column == 3 ? 0x42 : 0));
        }
    }
}

/* iterated over every column of the matrix while only the columns of one module are valid */
static void testRowOnAllModules()
{
    MaxMatrix Matrix(DataInPin, ChipSelectPin, ClockPin);
    initMatrix(Matrix);

    CHECK(Matrix.setRowOnAllModulesLL(2, 0xC1) == E_OK);
    for(byte Module = 0; Module < MAXMATRIX_NUMBER_OF_MODULES; Module++) {
        byte Row = 0;
        CHECK(Matrix.getRowLL(Module, 2, &Row) == E_OK);
        CHECK(Row == 0xC1);
    }
}

/* one address/value pair per module, and every module but the addressed one gets a no-op */
static void testColumnFrameAddressesOneModule()
{
    MaxMatrix Matrix(DataInPin, ChipSelectPin, ClockPin);
    initMatrix(Matrix);
    const byte Module = MAXMATRIX_NUMBER_OF_MODULES - 2;

    CHECK(Matrix.setColumnLL(Module, 3, 0x5A) == E_OK);
    CHECK(Mock::LastFrame.size() == 2u * MAXMATRIX_NUMBER_OF_MODULES);
    for(byte i = 0; i < MAXMATRIX_NUMBER_OF_MODULES && Mock::LastFrame.size() == 2u * MAXMATRIX_NUMBER_OF_MODULES; i++) {
        if(i == Module) {
            CHECK(Mock::LastFrame[2 * i] == MAX7219_REG_DIGIT3_ADDRESS);
            CHECK(Mock::LastFrame[2 * i + 1] == 0x5A);
        } else {
            CHECK(Mock::LastFrame[2 * i] == MAX7219_REG_NO_OP_ADDRESS);
        }
    }
}

/* chip select was not an output in SPI mode, and the SPI transaction was never ended */
static void testTransfersAreFramed()
{
    MaxMatrix Matrix(DataInPin, ChipSelectPin, ClockPin);
    initMatrix(Matrix);
    char Text[] = "Hello";

    CHECK(Mock::PinMode[ChipSelectPin] == OUTPUT);
    Matrix.setText("Hi!");
    Matrix.setModuleOrientation(MAXMATRIX_MODULE_ORIENTATION_90);
    shiftWholeText(Matrix, Text);

    CHECK(Mock::Frames > 0);
    CHECK(Mock::Errors == 0);
    CHECK(Mock::PinLevel[ChipSelectPin] == HIGH);
    CHECK(!Mock::TransactionOpen);
    CHECK(Mock::usesSpi() == (MAXMATRIX_USE_SPI == STD_ON));
}

static void testTextShiftFinishes()
{
    MaxMatrix Matrix(DataInPin, ChipSelectPin, ClockPin);
    initMatrix(Matrix);
    char Text[] = "Hello World!";

    shiftWholeText(Matrix, Text);
    CHECK(Matrix.getState() == MAXMATRIX_STATE_READY);
}

/* the Arduino IDE saves sketches as UTF-8, so an umlaut arrives as two bytes */
static void testUmlautsFromUtf8AndLatin1Match()
{
    MaxMatrix Matrix(DataInPin, ChipSelectPin, ClockPin);
    initMatrix(Matrix);
    byte Utf8[MAXMATRIX_BUFFER_SIZE], Latin1[MAXMATRIX_BUFFER_SIZE];

    Matrix.clear();
    CHECK(Matrix.setText("Äöü!") == E_OK);
    memcpy(Utf8, Matrix.MatrixBuffer, sizeof(Utf8));
    Matrix.clear();
    CHECK(Matrix.setText("\xC4\xF6\xFC!") == E_OK);
    memcpy(Latin1, Matrix.MatrixBuffer, sizeof(Latin1));
    CHECK(memcmp(Utf8, Latin1, sizeof(Utf8)) == 0);

    char ShiftUtf8[] = "Grüße Ä";
    char ShiftLatin1[] = "Gr\xFC\xDF" "e \xC4";
    shiftWholeText(Matrix, ShiftUtf8);
    CHECK(Matrix.getState() == MAXMATRIX_STATE_READY);
    memcpy(Utf8, Matrix.MatrixBuffer, sizeof(Utf8));
    shiftWholeText(Matrix, ShiftLatin1);
    memcpy(Latin1, Matrix.MatrixBuffer, sizeof(Latin1));
    CHECK(memcmp(Utf8, Latin1, sizeof(Utf8)) == 0);
}

static void testEveryUmlautHasItsOwnSprite()
{
    MaxMatrix Matrix(DataInPin, ChipSelectPin, ClockPin);
    const char* Umlauts[] = { "Ä", "Ö", "Ü", "ä", "ö", "ü" };
    char Char;
    spriteIndexType SpriteIndex;

    for(byte i = 0; i < 6; i++) {
        CHECK(Matrix.decodeChar(Umlauts[i], &Char) == 2);
        CHECK(Matrix.convertCharToSprite(Char, &SpriteIndex) == E_OK);
        CHECK(SpriteIndex == 95 + i);
    }

    /* the Ü sprite used to be a copy of the O */
    spriteType Ue, O;
    CHECK(Matrix.getSprite(97, &Ue) == E_OK);
    CHECK(Matrix.getSprite('O' + MAXMATRIX_ASCII_TABLE_OFFSET, &O) == E_OK);
    CHECK(memcmp(Ue, O, sizeof(Ue)) != 0);
}

static void testDecodeStopsAtEndOfString()
{
    MaxMatrix Matrix(DataInPin, ChipSelectPin, ClockPin);
    initMatrix(Matrix);
    char Char;

    CHECK(Matrix.decodeChar("A", &Char) == 1 && Char == 'A');
    CHECK(Matrix.decodeChar("\xC3", &Char) == 1);

    char Truncated[] = "a\xC3";
    shiftWholeText(Matrix, Truncated);
    CHECK(Matrix.getState() == MAXMATRIX_STATE_READY);
}

static void testUnknownChars()
{
    MaxMatrix Matrix(DataInPin, ChipSelectPin, ClockPin);
    initMatrix(Matrix);

    CHECK(Matrix.setText("a\x01") == E_NOT_OK);

    char Text[] = "a\x01" "b";
    shiftWholeText(Matrix, Text);
    CHECK(Matrix.getState() == MAXMATRIX_STATE_READY);
}

int main()
{
    struct { const char* Name; void (*Run)(); } const Tests[] = {
        { "row round trip in every orientation", testRowRoundTripInEveryOrientation },
        { "dot round trip in every orientation", testDotRoundTripInEveryOrientation },
        { "out of range changes nothing", testOutOfRangeChangesNothing },
        { "column on all modules", testColumnOnAllModules },
        { "row on all modules", testRowOnAllModules },
        { "column frame addresses one module", testColumnFrameAddressesOneModule },
        { "transfers are framed", testTransfersAreFramed },
        { "text shift finishes", testTextShiftFinishes },
        { "umlauts from UTF-8 and latin-1 match", testUmlautsFromUtf8AndLatin1Match },
        { "every umlaut has its own sprite", testEveryUmlautHasItsOwnSprite },
        { "decode stops at end of string", testDecodeStopsAtEndOfString },
        { "unknown chars", testUnknownChars },
    };

    printf("MaxMatrix tests (%s)\n", MAXMATRIX_USE_SPI == STD_ON ? "SPI" : "bit banging");
    for(const auto& Test : Tests) {
        int FailuresBefore = Failures;
        Test.Run();
        printf("%s %s\n", Failures == FailuresBefore ? "  ok  " : "  FAIL", Test.Name);
    }
    printf("%s\n", Failures == 0 ? "all tests passed" : "tests failed");
    return Failures == 0 ? 0 : 1;
}
