#!/bin/bash
# Runs SerialControl with --tcp and controls it as the control program does: each line sent to the TCP port reaches the
# sketch's Serial, and what the sketch answers comes back on the same connection, as from a board's serial port.
#
#   simulator/Test/TcpTest.sh SIMULATOR PORT [ARGUMENT...]
#
# The arguments go to the simulator: --elf FILE, for SimulatorAvr.
set -eu

Failed=0
Port=$2

fail() {
    echo "FAIL: $*" >&2
    Failed=1
}

# expect LINE ANSWER - sends a line and fails unless the next line that comes back is the answer; the sketch ends its
# lines with \r\n. SerialControl says "ready" once, at the end of setup(), which the client hears only if it connected
# before: SimulatorAvr boots in simulated time, which keeps to the host's clock, after the port has opened.
expect() {
    local Answer=""
    printf '%s\n' "$1" >&3
    IFS= read -r -t 2 Answer <&3 || true
    [ "$Answer" != $'ready\r' ] || IFS= read -r -t 2 Answer <&3 || true
    [ "$Answer" = "$2"$'\r' ] || fail "$1 got \"${Answer%$'\r'}\" rather than \"$2\""
}

# The simulator says on stdout that the port is open before setup() runs; waiting for that line rather than trying to
# connect, as a connection that is only a try would be taken for the client and keep the port for a while.
exec 4< <(exec "$1" "${@:3}" --tcp "$Port" --for 3)
SimulatorProcess=$!
IFS= read -r -t 5 Opened <&4 || true
[ "$Opened" = "Serial on localhost:$Port" ] || { echo "FAIL: the simulator did not open port $Port" >&2; exit 1; }
exec 3<>"/dev/tcp/127.0.0.1/$Port"

expect "text=Hi" "ok"
# "Hi" from the left edge, two hex digits per column, and the 24 empty columns after it
expect "picture" "picture=7F08087F00447D40$(printf '00%.0s' {1..24})"
expect "text=Gr"$'\xC3\xBC\xC3\x9F'"e" "ok"
expect "blink" "error: unknown command"
expect "intensity=16" "error: out of range"
expect "clear" "ok"

exec 3>&-
wait "$SimulatorProcess" || fail "the simulator exited with $?"
exit $Failed
