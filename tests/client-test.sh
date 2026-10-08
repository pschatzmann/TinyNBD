#!/usr/bin/env bash
# Starts the nbd test server and runs the NBDClient test against it.
# Usage: client-test.sh <path-to-nbd-test> <path-to-nbd-client-test>
set -u
SERVER="$1"
CLIENT="$2"
PORT=10809

"$SERVER" > server.log 2>&1 &
PID=$!
trap 'kill $PID 2>/dev/null' EXIT

# wait until the server accepts connections
for i in $(seq 50); do
  (exec 3<>/dev/tcp/127.0.0.1/$PORT) 2>/dev/null && break
  sleep 0.1
done

"$CLIENT"
RC=$?
if [ $RC -ne 0 ]; then
  echo "--- server log"
  cat server.log
fi
exit $RC
