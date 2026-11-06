#!/bin/bash
# Runs SerialControl with --web and controls it as the control page does: "serial LINE" over the WebSocket reaches the
# sketch's Serial as a line, and what the sketch answers comes back with the next panel, in "serial".
#
#   simulator/Test/ControlTest.sh SIMULATOR PORT [ARGUMENT...]
#
# The arguments go to the simulator: --elf FILE, for SimulatorAvr.
set -eu
source "$(dirname "$0")/WebTestLib.sh"

Port=$2
# Five seconds: the five answers below are each read for half a second, so the last command goes out 2.5 s after the
# start, and with 3 s a simulator slowed down by the tests around it exited before it answered.
startSimulator "$1" "$Port" 5 "${@:3}"
openWebSocket "$Port"
grep -q '^HTTP/1.1 101 ' <<< "$HandshakeAnswer" || fail "the WebSocket got no 101"

send "hello"
Panel=$(receive)
grep -q '"dots":"0*"' <<< "$Panel" || fail "SerialControl does not start with a dark panel"

# the sketch prints "ok" with println, so the answer ends in \r\n, which the JSON string carries as \u000D\u000A
send "serial text=Hi"
Panel=$(receive)
grep -q '"serial":"ok\\u000D\\u000A"' <<< "$Panel" || fail "text=Hi got no ok"
grep -q '"dots":"[01]*1' <<< "$Panel" || fail "text=Hi lit nothing"

send "serial blink"
Panel=$(receive)
grep -q '"serial":"error: unknown command\\u000D\\u000A"' <<< "$Panel" || fail "an unknown command got no error with its reason"

send "serial intensity=16"
Panel=$(receive)
grep -q '"serial":"error: out of range\\u000D\\u000A"' <<< "$Panel" || fail "intensity=16 got no error with its reason"

send "serial clear"
Panel=$(receive)
grep -q '"serial":"ok\\u000D\\u000A"' <<< "$Panel" || fail "clear got no ok"
grep -q '"dots":"0*"' <<< "$Panel" || fail "clear left LEDs lit"

finish
