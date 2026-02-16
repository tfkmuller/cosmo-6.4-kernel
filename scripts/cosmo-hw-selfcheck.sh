#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
#
# Quick runtime validation for the Cosmo Communicator 6.4 bring-up branch.

set -eu

fails=0

check() {
	name="$1"
	cmd="$2"
	if sh -c "$cmd" >/dev/null 2>&1; then
		printf '[OK] %s\n' "$name"
	else
		printf '[FAIL] %s\n' "$name"
		fails=$((fails + 1))
	fi
}

echo "Cosmo Communicator hardware self-check"
echo "====================================="

check "board model" "tr -d '\0' </proc/device-tree/model | grep -qi 'cosmo communicator'"
check "touch driver bound" "dmesg | grep -Eqi 'solomon|ssd20xx'"
check "keyboard matrix driver" "dmesg | grep -qi 'gpio-fastmatrix-keyboard'"
check "aw9523 gpio expanders" "dmesg | grep -qi 'aw9523'"
check "fusb301 type-c controller" "dmesg | grep -qi 'fusb301'"
check "modem scaffold probes" "dmesg | grep -qi 'mtk-cosmo-modem-scaffold|scaffold attached'"
check "touch input node" "grep -qi 'solomon' /proc/bus/input/devices"
check "keyboard input node" "grep -qi 'gpio-fastmatrix' /proc/bus/input/devices"
check "modem debugfs root" "[ -d /sys/kernel/debug/mtk-cosmo-modem ]"

echo
if [ "$fails" -eq 0 ]; then
	echo "Self-check passed."
	exit 0
fi

echo "Self-check failed: $fails checks did not pass."
echo "Inspect dmesg and /sys/kernel/debug/mtk-cosmo-modem for details."
exit 1
