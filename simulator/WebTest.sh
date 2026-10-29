#!/bin/bash
# Starts a simulator with --web and checks what a browser gets from it: the page, a 404 for any other path, and over
# the WebSocket the handshake and the panel. CI has no browser; this is the half of one the simulator talks to.
#
#   simulator/WebTest.sh SIMULATOR PORT
#
# The simulator runs with --for, so it stops on its own even when this script does not get as far as waiting for it.
set -eu

Simulator=$1
Port=$2
Failed=0

fail() {
    echo "FAIL: $*" >&2
    Failed=1
}

"$Simulator" --web --port "$Port" --for 3 > /dev/null &
Simulator=$!

# the port opens before setup() runs, but not before the process is up
for Try in $(seq 50); do
    curl -s -o /dev/null "http://localhost:$Port/" && break
    sleep 0.1
done

curl -s "http://localhost:$Port/" | grep -q '<title>MaxMatrix Simulator</title>' || fail "the page did not arrive"
Status=$(curl -s -o /dev/null -w '%{http_code}' "http://localhost:$Port/nothing" || true)
[ "$Status" = 404 ] || fail "an unknown path got $Status rather than 404"

# Basics shows "Hi!" from its first few milliseconds until two seconds in, and the simulator has sent that panel
# by now. In the quiet between, a page gets a panel only by asking for it with "hello" - which is what is tested.
sleep 0.5

# curl speaks no WebSocket, bash does over /dev/tcp. The key is the example in RFC 6455, which names its answer.
exec 3<>"/dev/tcp/127.0.0.1/$Port"
printf 'GET /ws HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n' >&3
printf 'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n' >&3
Answer=""
while IFS= read -r -t 2 Line <&3 && [ "$Line" != $'\r' ]; do
    Answer+="$Line"$'\n'
done
grep -q '^HTTP/1.1 101 ' <<< "$Answer" || fail "the WebSocket got no 101"
grep -q 's3pPLMBiTxaQ9kYGzzhZRbK+xOo=' <<< "$Answer" || fail "the WebSocket got the wrong Sec-WebSocket-Accept"

# A text frame as a browser sends it: masked, and with a mask that is not zeros, which would leave a host that
# unmasks wrongly undetected. Short texts only, the length has to fit in the second byte.
send() {
    local Text=$1 Mask=(0x12 0x34 0x56 0x78) Frame i
    Frame=$(printf '\\x81\\x%02x\\x12\\x34\\x56\\x78' $((0x80 | ${#Text})))
    for ((i = 0; i < ${#Text}; i++)); do
        Frame+=$(printf '\\x%02x' $(($(printf '%d' "'${Text:i:1}") ^ Mask[i % 4])))
    done
    printf "$Frame" >&3
}

# what arrives within half a second; the frame headers are binary and may hold a zero byte, which bash cannot keep
receive() {
    timeout 0.5 cat <&3 | tr -d '\0' || true
}

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
exec 3>&-

wait "$Simulator" || fail "the simulator exited with $?"
exit $Failed
