#!/usr/bin/env bash
# End-to-end proof of concept: daemon + fake DCS feed + mock swift client.
set -u
cd "$(dirname "$0")/.."

if [ ! -x build/dcsswiftbus ]; then
    echo "build first: cmake -B build && cmake --build build -j" >&2
    exit 1
fi

./build/dcsswiftbus &
DAEMON_PID=$!
python3 test/mock_dcs.py &
MOCK_PID=$!
trap 'kill $DAEMON_PID $MOCK_PID 2>/dev/null' EXIT

sleep 2
./build/mock_swift_client
RC=$?
exit $RC
