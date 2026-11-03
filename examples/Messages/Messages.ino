/*
 * Messages
 *
 * Scrolls several texts one after the other, each at a speed of its own, without blocking
 * loop(). Umlauts can be typed as they are: the library reads the UTF-8 the Arduino IDE saves.
 *
 * Wiring (Arduino Uno / Nano):
 *   DIN -> 11, CS -> 10, CLK -> 13
 */
#include <MaxMatrix.h>

/* a text must stay valid while it is shifted, so none of them is a local variable; the blanks at
 * the end let it leave the matrix before the next one comes in */
char Hello[] = "Hello!    ";
char Umlauts[] = "Grüne Äpfel, schöne Öfen    ";
char Fast[] = "fast...    ";
char Slow[] = "and slow    ";

struct MessageType {
    char* Text;
    uint16_t ShiftIntervalMs;
};

MessageType const Messages[] = {
    { Hello, 50 },
    { Umlauts, 50 },
    { Fast, 15 },
    { Slow, 120 },
};
byte const NUMBER_OF_MESSAGES = sizeof(Messages) / sizeof(Messages[0]);

MaxMatrix Matrix(11, 10, 13);
byte Current = 0;

void show(MessageType const& Message)
{
    Matrix.setShiftInterval(Message.ShiftIntervalMs);
    Matrix.setTextWithShift(Message.Text);
}

void setup()
{
    Matrix.init();
    Matrix.setModuleOrientation(MAXMATRIX_MODULE_ORIENTATION_270);    // FC-16, see the README
    Matrix.setIntensity(2);
    show(Messages[Current]);
}

void loop()
{
    /* takes a step only when the interval of the current text has passed */
    Matrix.shiftTask();
    /* the text has been shifted in completely: on to the next one */
    if(Matrix.getState() == MAXMATRIX_STATE_READY) {
        Current = (Current + 1) % NUMBER_OF_MESSAGES;
        show(Messages[Current]);
    }
}
