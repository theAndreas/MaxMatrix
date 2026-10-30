/* Runs an example sketch from examples/ on the host and draws the LED panel in the terminal.
 *
 *   build/Simulator-ScrollText-bitbang [--wiring FC16] [--upside-down] [--web [--port PORT]] [--for SECONDS [--expect FILE]]
 *
 * The sketch is compiled unchanged against the Arduino stand-in and the MAX7219 model in test/mock/: setup() runs
 * once, loop() over and over, millis() and delay() on the host clock. The panel is drawn from the model's LEDs as
 * the chosen module would light them, so a sketch with the wrong orientation for that module shows it wrong here
 * too - which is the point, rather than drawing MatrixBuffer.
 *
 * Without --for it runs until Ctrl+C and redraws the panel in place. With --for it runs that long, prints the panel
 * once and exits; --expect then compares the printout with a file and fails on a difference.
 *
 * --web draws in a browser instead, on http://localhost:8080/ (simulator/index.html): the panel with the brightness
 * of every module, the frames on the wire, and the wiring and mounting to choose while the sketch runs. --port picks
 * another port, so that two simulators, or a simulator and the tests, can run side by side. */
#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include "Arduino.h"
#include "MaxMatrix.h"
#include "WebHost.h"

void setup();
void loop();

namespace {

/* the examples put chip select on pin 10 */
const int ChipSelectPin = 10;
/* about 30 pictures a second; neither a terminal nor a page keeps up with every frame */
const unsigned long RedrawInterval = 33;
/* the most frames the page is sent at once; more would scroll past before anyone read them */
const size_t FrameLogLength = 16;

const struct {
    const char* Name;
    const Max7219WiringType* Wiring;
} NamedWirings[] = {
    { "FC16", &MAX7219_WIRING_FC16 },
    { "GENERIC", &MAX7219_WIRING_GENERIC },
    { "PAROLA", &MAX7219_WIRING_PAROLA },
    { "ICSTATION", &MAX7219_WIRING_ICSTATION },
};

/* what the command line chooses; the wiring and the mounting the page can change later */
std::string WiringName = "FC16";
const Max7219WiringType* Wiring = &MAX7219_WIRING_FC16;
bool UpsideDown = false;
bool Web = false;
unsigned short Port = 8080;
unsigned long RunFor = 0;
const char* ExpectFile = nullptr;

/* what was drawn last, so that a picture is only drawn again when it changed */
int DrawnFrames = -1;
unsigned long LastDraw = 0;
bool SettingsChanged = true;

/* the frames since the page was last sent the panel, as hex pairs, one string per frame */
std::vector<std::string> FrameLog;
int LoggedFrames = 0;

const Max7219WiringType* findWiring(const std::string& Name)
{
    for(const auto& Named : NamedWirings) {
        if(Name == Named.Name) return Named.Wiring;
    }
    for(const Max7219WiringType& Each : Max7219Wirings) {
        if(Name == Each.Name) return &Each;
    }
    return nullptr;
}

/* the panel as text, row after row; Lit and Dark are one dot each, EndOfRow follows every row */
std::string panel(const char* Lit, const char* Dark, const char* EndOfRow)
{
    std::string Text;

    for(unsigned Y = 0; Y < MAXMATRIX_ROW_NUMBER_OF_MODULE; Y++) {
        for(unsigned X = 0; X < MAXMATRIX_NUMBER_OF_COLUMNS; X++) Text += Mock::Chain.isLit(*Wiring, X, Y, UpsideDown) ? Lit : Dark;
        Text += EndOfRow;
    }
    return Text;
}

/* redraws in place: cursor home rather than clearing, which would flicker */
void draw()
{
    printf("\x1b[H%s, %d modules%s, intensity", WiringName.c_str(), MAXMATRIX_NUMBER_OF_MODULES, UpsideDown ? ", upside down" : "");
    for(size_t Chip = Mock::Chain.Chips.size(); Chip > 0; Chip--) printf(" %d", Mock::Chain.Chips[Chip - 1].Intensity);
    printf(", %d frames%s\x1b[K\n\n", Mock::Frames, Mock::Errors ? ", TRANSFER ERRORS" : "");
    fputs(panel("\u2588\u2588", "\u00b7 ", "\n").c_str(), stdout);
    fflush(stdout);
}

/* the end of a run with --for: the panel once, and the exit code says whether it is the one expected */
void finish()
{
    std::string Picture = panel("#", ".", "\n");

    fputs(Picture.c_str(), stdout);
    if(ExpectFile != nullptr) {
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

/* tick() follows every frame, so each one gets logged; the page shows them as they pass */
void logFrame()
{
    std::string Line = "#" + std::to_string(Mock::Frames) + ":";
    char Pair[8];

    for(size_t i = 0; i + 1 < Mock::LastFrame.size(); i += 2) {
        snprintf(Pair, sizeof(Pair), "%02X %02X", Mock::LastFrame[i], Mock::LastFrame[i + 1]);
        Line += i == 0 ? " " : " | ";
        Line += Pair;
    }
    FrameLog.push_back(Line);
    if(FrameLog.size() > FrameLogLength) FrameLog.erase(FrameLog.begin());
    LoggedFrames = Mock::Frames;
}

/* the panel for the page, the brightness of each module position from the left and the frames since last time */
void sendToPage()
{
    std::ostringstream Json;

    Json << "{\"wiring\":\"" << WiringName << "\",\"upsideDown\":" << (UpsideDown ? "true" : "false")
         << ",\"columns\":" << MAXMATRIX_NUMBER_OF_COLUMNS << ",\"rows\":" << MAXMATRIX_ROW_NUMBER_OF_MODULE
         << ",\"dots\":\"" << panel("1", "0", "") << "\",\"intensity\":[";
    for(unsigned Module = 0; Module < MAXMATRIX_NUMBER_OF_MODULES; Module++) {
        Json << (Module > 0 ? "," : "")
             << static_cast<unsigned>(Mock::Chain.chipAt(Module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE, UpsideDown).Intensity);
    }
    Json << "],\"frames\":" << Mock::Frames << ",\"errors\":" << Mock::Errors << ",\"log\":[";
    for(size_t i = 0; i < FrameLog.size(); i++) Json << (i > 0 ? "," : "") << '"' << FrameLog[i] << '"';
    Json << "]}";

    WebHost::broadcast(Json.str());
    FrameLog.clear();
}

/* what the page sends: "wiring NAME", "upside-down 0|1", and "hello" when it opens, which asks for the panel */
void command(const std::string& Text)
{
    const std::string WiringCommand = "wiring ";

    if(Text.compare(0, WiringCommand.size(), WiringCommand) == 0) {
        std::string Name = Text.substr(WiringCommand.size());
        const Max7219WiringType* Found = findWiring(Name);
        if(Found != nullptr) {
            Wiring = Found;
            WiringName = Name;
        }
    } else if(Text == "upside-down 1") {
        UpsideDown = true;
    } else if(Text == "upside-down 0") {
        UpsideDown = false;
    }
    SettingsChanged = true;
}

/* called after every frame and every millisecond of delay() */
void tick()
{
    unsigned long Now = millis();

    if(Web) {
        if(Mock::Frames != LoggedFrames) logFrame();
        WebHost::task(command);
    }
    if(RunFor && Now >= RunFor) finish();

    bool Changed = Mock::Frames != DrawnFrames || SettingsChanged;
    if(!Changed || Now - LastDraw < RedrawInterval) return;
    if(Web) sendToPage();
    else if(!RunFor) draw();
    DrawnFrames = Mock::Frames;
    LastDraw = Now;
    SettingsChanged = false;
}

void usage(const char* Program)
{
    fprintf(stderr, "usage: %s [--wiring NAME] [--upside-down] [--web [--port PORT]] [--for SECONDS [--expect FILE]]\n", Program);
    fprintf(stderr, "  NAME: FC16 (default), GENERIC, PAROLA, ICSTATION or DR0CR0RR0 ... DR1CR1RR1\n");
    exit(2);
}

}

int main(int argc, char** argv)
{
    for(int i = 1; i < argc; i++) {
        const std::string Option = argv[i];
        const bool HasValue = i + 1 < argc;

        if(Option == "--wiring" && HasValue) {
            WiringName = argv[++i];
            Wiring = findWiring(WiringName);
            if(Wiring == nullptr) usage(argv[0]);
        } else if(Option == "--upside-down") {
            UpsideDown = true;
        } else if(Option == "--web") {
            Web = true;
        } else if(Option == "--port" && HasValue) {
            Port = static_cast<unsigned short>(atoi(argv[++i]));
            if(Port == 0) usage(argv[0]);
        } else if(Option == "--for" && HasValue) {
            RunFor = static_cast<unsigned long>(atof(argv[++i]) * 1000);
            if(RunFor == 0) usage(argv[0]);
        } else if(Option == "--expect" && HasValue) {
            ExpectFile = argv[++i];
        } else {
            usage(argv[0]);
        }
    }
    if(ExpectFile != nullptr && RunFor == 0) usage(argv[0]);

    Mock::reset(ChipSelectPin, MAXMATRIX_NUMBER_OF_MODULES);
    Mock::Tick = tick;
    if(Web) {
        if(!WebHost::begin(Port, SIMULATOR_PAGE)) {
            fprintf(stderr, "port %u is taken, maybe by another simulator\n", Port);
            return 1;
        }
        printf("http://localhost:%u/\n", Port);
        fflush(stdout);
    } else if(!RunFor) {
        printf("\x1b[2J");
    }

    setup();
    for(;;) {
        loop();
        tick();
        /* a sketch polling millis() would otherwise keep a core busy */
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
