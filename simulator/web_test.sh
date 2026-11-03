#!/bin/bash
# Starts a simulator with --web and checks what a browser gets from it: the page, a 404 for any other path, and over
# the WebSocket the handshake and the panel. CI has no browser; this is the half of one the simulator talks to.
#
#   simulator/web_test.sh SIMULATOR PORT
set -eu
source "$(dirname "$0")/web_test_lib.sh"

Port=$2
startSimulator "$1" "$Port" 3

curl -s "http://localhost:$Port/" | grep -q '<title>MaxMatrix Simulator</title>' || fail "the page did not arrive"
Status=$(curl -s -o /dev/null -w '%{http_code}' "http://localhost:$Port/nothing" || true)
[ "$Status" = 404 ] || fail "an unknown path got $Status rather than 404"

# Basics shows "Hi!" from its first few milliseconds until two seconds in, and the simulator has sent that panel
# by now. In the quiet between, a page gets a panel only by asking for it with "hello" - which is what is tested.
sleep 0.5

openWebSocket "$Port"
grep -q '^HTTP/1.1 101 ' <<< "$HandshakeAnswer" || fail "the WebSocket got no 101"
grep -q 's3pPLMBiTxaQ9kYGzzhZRbK+xOo=' <<< "$HandshakeAnswer" || fail "the WebSocket got the wrong Sec-WebSocket-Accept"

# Only after the 101: the host takes the handshake as one packet and would drop a frame that came with it, which a
# browser never does. Every command answers with the panel, so "hello" is followed by one that changes it.
send "hello"
Panel=$(receive)
grep -q '"wiring":"FC16"' <<< "$Panel" || fail "no panel after hello"
grep -q '"dots":"[01]*1' <<< "$Panel" || fail "the panel is dark"
grep -q '"errors":0,' <<< "$Panel" || fail "the panel reports transfer errors"
Dots=$(grep -o '"dots":"[01]*"' <<< "$Panel" || true)

# the sketch draws for an FC-16 board, so on a GENERIC module the same LEDs make a different picture
send "wiring GENERIC"
Panel=$(receive)
grep -q '"wiring":"GENERIC"' <<< "$Panel" || fail "the wiring did not change"
[ "$(grep -o '"dots":"[01]*"' <<< "$Panel" || true)" != "$Dots" ] || fail "the picture did not change with the wiring"

finish
