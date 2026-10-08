#!/usr/bin/env bash
# Starts the nbd test server and runs qemu-img / qemu-io against it.
# Usage: qemu-test.sh <path-to-nbd-test>
set -u
SERVER="$1"
URL=nbd://127.0.0.1:10809
FAILED=0

"$SERVER" > server.log 2>&1 &
PID=$!
trap 'kill $PID 2>/dev/null' EXIT
for i in $(seq 50); do
  qemu-img info "$URL/ram" > /dev/null 2>&1 && break
  sleep 0.1
done

check() {
  local name="$1"; shift
  if "$@" > out.log 2>&1 && ! grep -q "Pattern verification failed\|failed" out.log; then
    echo "PASS: $name"
  else
    echo "FAIL: $name"; cat out.log; FAILED=1
  fi
}
fails() {
  local name="$1"; shift
  if "$@" > out.log 2>&1; then echo "FAIL: $name"; cat out.log; FAILED=1; else echo "PASS: $name"; fi
}

head -c 4194304 /dev/urandom > random.bin
check "size" bash -c "qemu-img info --output=json $URL/ram | grep -q '\"virtual-size\": 4194304'"
check "write & compare ram" bash -c "qemu-img convert -n -O raw random.bin $URL/ram && qemu-img compare random.bin $URL/ram"
check "write & compare image" bash -c "qemu-img convert -n -O raw random.bin $URL/image && qemu-img compare random.bin $URL/image"
check "unaligned io" qemu-io -f raw -c "write -P 0xab 1000 10000" -c "read -P 0xab 1000 10000" \
  -c "read -P 0 0 1000" -c "write -z 5000 3000" -c "read -P 0 5000 3000" -c "read -P 0xab 8000 3000" -c "flush" "$URL/sect"
check "trim" qemu-io -f raw -c "discard 0 65536" -c "read -P 0 0 65536" "$URL/ram"
check "read-only read" qemu-io -f raw -r -c "read 0 512" "$URL/ro"
fails "read-only write" qemu-io -f raw -c "write -P 1 0 512" "$URL/ro"
fails "unknown export" qemu-img info "$URL/nope"
check "default export" qemu-img info "$URL"

if [ $FAILED -ne 0 ]; then echo "--- server log"; cat server.log; fi
exit $FAILED
