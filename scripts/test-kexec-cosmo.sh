#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
#
# Non-destructive Cosmo kernel test via kexec.
# Loads a kernel from the source tree and optionally jumps into it.

set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
SRC_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)"

IMAGE="$SRC_ROOT/arch/arm64/boot/Image.gz"
DTB="$SRC_ROOT/arch/arm64/boot/dts/mediatek/mt6771-planet-cosmocom.dtb"
CMDLINE="console=tty0 console=ttyS0,921600n1 root=/dev/mmcblk0p43 rw rootwait"
EXEC_NOW=0

usage() {
	cat <<EOF
Usage: sudo $0 [--exec] [--image PATH] [--dtb PATH] [--cmdline STRING]

Options:
  --exec           Immediately switch into test kernel after loading it.
  --image PATH     Kernel image path (default: $IMAGE).
  --dtb PATH       DTB path (default: $DTB).
  --cmdline STR    Kernel command line.
  -h, --help       Show this help.

Default behavior is load-only (safe). Run 'sudo systemctl kexec' when ready.
EOF
}

while [ "$#" -gt 0 ]; do
	case "$1" in
		--exec)
			EXEC_NOW=1
			shift
			;;
		--image)
			IMAGE="$2"
			shift 2
			;;
		--dtb)
			DTB="$2"
			shift 2
			;;
		--cmdline)
			CMDLINE="$2"
			shift 2
			;;
		-h|--help)
			usage
			exit 0
			;;
		*)
			echo "Unknown argument: $1" >&2
			usage >&2
			exit 2
			;;
	esac
done

if [ "$(id -u)" -ne 0 ]; then
	echo "Run as root (use sudo)." >&2
	exit 1
fi

if ! command -v kexec >/dev/null 2>&1; then
	echo "kexec not found. Install it with: apt-get install -y kexec-tools" >&2
	exit 1
fi

if [ ! -f "$IMAGE" ]; then
	echo "Kernel image not found: $IMAGE" >&2
	exit 1
fi

if [ ! -f "$DTB" ]; then
	echo "DTB not found: $DTB" >&2
	exit 1
fi

echo "Loading test kernel via kexec..."
echo "  image:   $IMAGE"
echo "  dtb:     $DTB"
echo "  cmdline: $CMDLINE"
kexec -l "$IMAGE" --dtb="$DTB" --command-line="$CMDLINE"

if [ "$EXEC_NOW" -eq 0 ]; then
	echo
	echo "Kernel loaded. This has not replaced your flashed boot image."
	echo "When ready, switch with:"
	echo "  sudo systemctl kexec"
	echo "If systemctl path fails, use:"
	echo "  sudo kexec -e"
	exit 0
fi

echo "Switching now via kexec..."
sync
if command -v systemctl >/dev/null 2>&1; then
	systemctl kexec
else
	kexec -e
fi
