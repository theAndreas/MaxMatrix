// Runs the script of index.html against stand-ins for the DOM, the WebSocket and a board on Web Serial, and checks what
// it sends where. CI has no browser; web_test.sh and control_test.sh stand in for the page towards the simulator, and
// this is the page itself.
//
//   node simulator/page_test.js simulator/index.html
//
// Needs Node 18 or newer, for ReadableStream, WritableStream and TextDecoderStream without an import.
"use strict";
const fs = require("fs");
const vm = require("vm");

const Html = fs.readFileSync(process.argv[2], "utf8");
const Script = Html.slice(Html.indexOf("<script>") + "<script>".length, Html.indexOf("</script>"));
const Elements = {};
const SentToSimulator = [];
const SentToBoard = [];
const Results = [];
let Socket = null;
let Board = null;
let PortClosed = false;

function check(Passed, What) {
    Results.push(`${Passed ? "ok  " : "FAIL"} ${What}`);
}

/* every element the script asks for, with what it reads and writes of one */
function element(Id) {
    if (!Elements[Id]) {
        Elements[Id] = {
            value: "", checked: false, disabled: false, textContent: "", innerHTML: "",
            scrollTop: 0, clientHeight: 100, scrollHeight: 100, width: 960, height: 240, handlers: {},
            addEventListener(Type, Handler) { this.handlers[Type] = Handler; },
            getContext() { return { clearRect() {}, beginPath() {}, arc() {}, fill() {} }; },
            getBoundingClientRect() { return { left: 0, top: 0, width: this.width, height: this.height }; },
        };
    }
    return Elements[Id];
}

class FakeWebSocket {
    constructor() {
        this.readyState = FakeWebSocket.OPEN;
        Socket = this;
        setTimeout(() => this.onopen(), 0);
    }
    send(Message) { SentToSimulator.push(Message); }
}
FakeWebSocket.OPEN = 1;

/* a port whose readable side the test writes into, as a board would */
const Port = {
    async open(Options) { this.baudRate = Options.baudRate; },
    readable: new ReadableStream({ start(Controller) { Board = Controller; } }),
    writable: new WritableStream({ write(Chunk) { SentToBoard.push(Buffer.from(Chunk).toString()); } }),
    /* as in Chrome: a port does not close while its streams are still locked */
    async close() {
        if (this.readable.locked || this.writable.locked) throw new TypeError("the port has a locked stream");
        PortClosed = true;
    },
};

function boardPrints(Text) {
    Board.enqueue(new TextEncoder().encode(Text));
}

function panel(Serial, Dots) {
    return { data: JSON.stringify({ wiring: "FC16", upsideDown: false, columns: 32, rows: 8, dots: Dots,
                                    intensity: [5, 5, 5, 5], frames: 1, errors: 0, log: [], serial: Serial }) };
}

/* the centre of an LED on the canvas: 24 pixels a cell and a gap of 8 after every module */
function clickOn(Column, Row) {
    Elements.matrix.handlers.click({ clientX: Column * 24 + Math.floor(Column / 8) * 8 + 12, clientY: Row * 24 + 12 });
}

const settle = () => new Promise(Resolve => setTimeout(Resolve, 10));

async function run() {
    element("text").value = "Hello";
    element("intensity").value = "7";
    const Context = vm.createContext({
        document: { getElementById: element, body: {} },
        getComputedStyle: () => ({ getPropertyValue: () => "" }),
        WebSocket: FakeWebSocket,
        location: { host: "localhost:8080" },
        navigator: { serial: { requestPort: async () => Port } },
        TextEncoder, TextDecoderStream, setTimeout, JSON, Math,
    });
    vm.runInContext(Script, Context);
    await settle();
    check(SentToSimulator[0] === "hello", "the page says hello when the WebSocket opens");

    /* the sketch's answers arrive with the panels, and a line may be split between two */
    const Dots = "0".repeat(3 * 32 + 5) + "1" + "0".repeat(256 - 3 * 32 - 6);
    Socket.onmessage(panel("ok\r", Dots));
    check(!Elements.answers.textContent.includes("simulator: ok"), "half a line is held back");
    Socket.onmessage(panel("\nerror\r\n", Dots));
    check(Elements.answers.textContent.includes("simulator: ok\nsimulator: error"), "a line in two pieces is put together");

    Elements.showText.handlers.click();
    check(SentToSimulator.at(-1) === "serial text=Hello", "Show sends text=");
    Elements.shiftText.handlers.click();
    check(SentToSimulator.at(-1) === "serial textShift=Hello", "Shift in sends textShift=");
    Elements.intensity.handlers.change();
    check(SentToSimulator.at(-1) === "serial intensity=7", "the slider sends intensity=");

    /* LED 5,3 is lit in the panel above */
    clickOn(5, 3);
    check(SentToSimulator.at(-1) === "serial setDot=5,3,0", "a click on a lit LED switches it off");
    clickOn(9, 0);
    check(SentToSimulator.at(-1) === "serial setDot=9,0,1", "a click in module 1 counts the gap before it");
    const Count = SentToSimulator.length;
    Elements.matrix.handlers.click({ clientX: 8 * 24 + 4, clientY: 12 });
    check(SentToSimulator.length === Count, "a click into the gap between modules sends nothing");

    /* opening the port resets an Uno: nothing goes to the board until SerialControl says ready */
    await Elements.board.handlers.click();
    await settle();
    check(Port.baudRate === 115200, "the board is opened at 115200 baud");
    Elements.clear.handlers.click();
    await settle();
    check(SentToBoard.length === 0, "nothing goes to the board before it is ready");
    boardPrints("rea");
    boardPrints("dy\r\n");
    await settle();
    check(Elements.boardStatus.textContent === "board connected", "ready, in two pieces, is seen");
    Elements.clear.handlers.click();
    await settle();
    check(SentToBoard.at(-1) === "clear\n", "a command reaches the board as a line");
    check(SentToSimulator.at(-1) === "serial clear", "and the simulator as well");
    boardPrints("ok\r\n");
    await settle();
    check(Elements.answers.textContent.includes("board: ok"), "the board's answer is shown");

    Elements.board.handlers.click();
    await settle();
    await settle();
    check(PortClosed, "disconnecting closes the port");
    check(Elements.board.textContent === "Connect board", "and the button offers to connect again");

    console.log(Results.join("\n"));
    process.exit(Results.some(Result => Result.startsWith("FAIL")) ? 1 : 0);
}

run();
