# Shared by WebTest.sh and ControlTest.sh: a simulator started with --web, and the half of a browser that talks to it.
# curl speaks no WebSocket, so the WebSocket runs over bash's /dev/tcp on file descriptor 3.
#
#   source "$(dirname "$0")/WebTestLib.sh"

Failed=0

fail() {
    echo "FAIL: $*" >&2
    Failed=1
}

# startSimulator SIMULATOR PORT SECONDS [ARGUMENT...] - runs it with the arguments and --for, so it stops on its own even when a test does not get as
# far as waiting for it; returns once the port answers, which it does before setup() runs
startSimulator() {
    "$1" "${@:4}" --web --port "$2" --for "$3" > /dev/null &
    SimulatorProcess=$!
    for Try in $(seq 50); do
        curl -s -o /dev/null "http://localhost:$2/" && return 0
        sleep 0.1
    done
}

# openWebSocket PORT - the handshake with the key from the example in RFC 6455, which names its answer; the headers of
# the answer end up in HandshakeAnswer
openWebSocket() {
    local Line
    exec 3<>"/dev/tcp/127.0.0.1/$1"
    printf 'GET /ws HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n' >&3
    printf 'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n' >&3
    HandshakeAnswer=""
    while IFS= read -r -t 2 Line <&3 && [ "$Line" != $'\r' ]; do
        HandshakeAnswer+="$Line"$'\n'
    done
}

# send TEXT - a text frame as a browser sends it: masked, and with a mask that is not zeros, which would leave a host
# that unmasks wrongly undetected. Short texts only, the length has to fit in the second byte.
send() {
    local Text=$1 Mask=(0x12 0x34 0x56 0x78) Frame i
    Frame=$(printf '\\x81\\x%02x\\x12\\x34\\x56\\x78' $((0x80 | ${#Text})))
    for ((i = 0; i < ${#Text}; i++)); do
        Frame+=$(printf '\\x%02x' $(($(printf '%d' "'${Text:i:1}") ^ Mask[i % 4])))
    done
    printf "$Frame" >&3
}

# receive - what arrives within half a second; the frame headers are binary and may hold a zero byte, which bash cannot
# keep
receive() {
    timeout 0.5 cat <&3 | tr -d '\0' || true
}

# finish - closes the WebSocket, waits for the simulator and ends the test with what it found
finish() {
    exec 3>&-
    wait "$SimulatorProcess" || fail "the simulator exited with $?"
    exit $Failed
}
