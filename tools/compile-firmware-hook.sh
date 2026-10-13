#!/usr/bin/env bash
# PostToolUse hook: compile the sketch when a file under firmware/ was edited.
input=$(cat)
case "$input" in *firmware/car_alarm*|*firmware\\car_alarm*) ;; *) exit 0 ;; esac
CLI="/c/Program Files/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"
cd "$(dirname "$0")/.." || exit 0
out=$("$CLI" compile --fqbn arduino:avr:uno --config-file ~/.arduinoIDE/arduino-cli.yaml firmware/car_alarm 2>&1)
if [ $? -ne 0 ]; then echo "$out" | tail -20 >&2; exit 2; fi
echo "$out" | grep -E "Sketch uses|Global variables" >&2
exit 0
