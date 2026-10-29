/* Runs an example sketch from examples/ on the host and draws the LED panel in the terminal.
 *
 *   build/Simulator-ScrollText-bitbang [--wiring FC16] [--upside-down] [--for SECONDS [--expect FILE]]
 *
 * The sketch is compiled unchanged against the Arduino stand-in and the MAX7219 model in test/mock/: setup() runs
 * once, loop() over and over, millis() and delay() on the host clock. The panel is drawn from the model's LEDs as
 * the chosen module would light them, so a sketch with the wrong orientation for that module shows it wrong here
 * too - which is the point, rather than drawing MatrixBuffer.
 *
 * Without --for it runs until Ctrl+C and redraws the panel in place. With --for it runs that long, prints the panel
 * once and exits; --expect then compares the printout with a file and fails on a difference. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include "Arduino.h"
#include "MaxMatrix.h"

void setup();
void loop();

static const struct {
    const char* Name;
    const Max7219WiringType* Wiring;
} NamedWirings[] = {
    { "FC16", &MAX7219_WIRING_FC16 },
    { "GENERIC", &MAX7219_WIRING_GENERIC },
    { "PAROLA", &MAX7219_WIRING_PAROLA },
    { "ICSTATION", &MAX7219_WIRING_ICSTATION },
};

static const char* WiringName = "FC16";
static const Max7219WiringType* Wiring = &MAX7219_WIRING_FC16;
static bool UpsideDown = false;
static unsigned long RunFor = 0;
static const char* ExpectFile = NULL;
static int DrawnFrames = -1;
static unsigned long LastDraw = 0;

/* the panel as text, one line per row; Lit and Dark are one dot each */
static std::string panel(const char* Lit, const char* Dark)
{
    std::string Text;
    for(unsigned Y = 0; Y < MAXMATRIX_ROW_NUMBER_OF_MODULE; Y++) {
        for(unsigned X = 0; X < MAXMATRIX_NUMBER_OF_COLUMNS; X++) Text += Mock::Chain.isLit(*Wiring, X, Y, UpsideDown) ? Lit : Dark;
        Text += '\n';
    }
    return Text;
}

/* redraws in place: cursor home rather than clearing, which would flicker */
static void draw()
{
    printf("\x1b[H%s, %d modules%s, intensity", WiringName, MAXMATRIX_NUMBER_OF_MODULES, UpsideDown ? ", upside down" : "");
    for(size_t Chip = Mock::Chain.Chips.size(); Chip > 0; Chip--) printf(" %d", Mock::Chain.Chips[Chip - 1].Intensity);
    printf(", %d frames%s\x1b[K\n\n", Mock::Frames, Mock::Errors ? ", TRANSFER ERRORS" : "");
    fputs(panel("\u2588\u2588", "\u00b7 ").c_str(), stdout);
    fflush(stdout);
    DrawnFrames = Mock::Frames;
    LastDraw = millis();
}

static void finish()
{
    std::string Picture = panel("#", ".");

    fputs(Picture.c_str(), stdout);
    if(ExpectFile != NULL) {
        std::ifstream File(ExpectFile);
        std::stringstream Expected;
        Expected << File.rdbuf();
        if(!File || Expected.str() != Picture) {
            fprintf(stderr, "differs from %s:\n%s", ExpectFile, Expected.str().c_str());
            exit(1);
        }
    }
    exit(Mock::Errors ? 1 : 0);
}

/* called after every frame and every millisecond of delay(); a terminal cannot keep up with every frame */
static void tick()
{
    unsigned long Now = millis();

    if(RunFor) {
        if(Now >= RunFor) finish();
    } else if(Mock::Frames != DrawnFrames && Now - LastDraw >= 33) {
        draw();
    }
}

static void usage(const char* Program)
{
    fprintf(stderr, "usage: %s [--wiring NAME] [--upside-down] [--for SECONDS [--expect FILE]]\n", Program);
    fprintf(stderr, "  NAME: FC16 (default), GENERIC, PAROLA, ICSTATION or DR0CR0RR0 ... DR1CR1RR1\n");
    exit(2);
}

static const Max7219WiringType* findWiring(const char* Name)
{
    for(const auto& Named : NamedWirings) {
        if(strcmp(Named.Name, Name) == 0) return Named.Wiring;
    }
    for(const Max7219WiringType& Each : Max7219Wirings) {
        if(strcmp(Each.Name, Name) == 0) return &Each;
    }
    return NULL;
}

int main(int argc, char** argv)
{
    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--wiring") == 0 && i + 1 < argc) {
            WiringName = argv[++i];
            Wiring = findWiring(WiringName);
            if(Wiring == NULL) usage(argv[0]);
        } else if(strcmp(argv[i], "--upside-down") == 0) {
            UpsideDown = true;
        } else if(strcmp(argv[i], "--for") == 0 && i + 1 < argc) {
            RunFor = (unsigned long)(atof(argv[++i]) * 1000);
            if(RunFor == 0) usage(argv[0]);
        } else if(strcmp(argv[i], "--expect") == 0 && i + 1 < argc) {
            ExpectFile = argv[++i];
        } else {
            usage(argv[0]);
        }
    }
    if(ExpectFile != NULL && RunFor == 0) usage(argv[0]);

    /* the examples select chip select on pin 10 */
    Mock::reset(10, MAXMATRIX_NUMBER_OF_MODULES);
    Mock::Tick = tick;
    if(!RunFor) printf("\x1b[2J");

    setup();
    for(;;) {
        loop();
        tick();
        /* a sketch polling millis() would otherwise keep a core busy */
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
