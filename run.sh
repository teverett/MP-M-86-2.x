#!/bin/sh
# run.sh -- build the bootable MP/M-86 floppy image and boot it in QEMU.
#
#   ./run.sh              build what's out of date, then boot
#   ./run.sh --clean      rebuild everything from scratch first
#   ./run.sh --headless   no QEMU window (the boot sector's BIOS messages
#                         go to it; everything after is on the serial port)
#   ./run.sh --img PATH   also copy the image to PATH
#   ./run.sh --build-only build the image, don't boot it
#
# The console is QEMU's serial port (COM1), on this terminal: MPMLDR and
# MP/M-86 itself talk to the UART directly. Ctrl-A X quits QEMU, Ctrl-A C
# toggles its monitor. The list device (LPT1) prints to src-assem/build/printer.txt.
set -e

cd "$(dirname "$0")"
IMG=src-assem/build/disk/mpm86q.img
clean=0 headless=0 boot=1 copy=

while [ $# -gt 0 ]; do
    case "$1" in
        --clean) clean=1 ;;
        --headless) headless=1 ;;
        --build-only) boot=0 ;;
        --img) shift; copy="$1" ;;
        -h|--help) sed -n '2,14p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "run.sh: unknown option '$1' (try --help)" >&2; exit 2 ;;
    esac
    shift
done

command -v qemu-system-i386 >/dev/null 2>&1 || [ "$boot" = 0 ] || {
    echo "run.sh: qemu-system-i386 not found (brew install qemu)" >&2; exit 1; }

if [ "$clean" = 1 ]; then
    make -C src-assem clean >/dev/null
fi
echo "run.sh: building..."
make -C src-assem >/dev/null
make -C src-assem disk >/dev/null
echo "run.sh: $IMG ($(wc -c < "$IMG" | tr -d ' ') bytes)"

if [ -n "$copy" ]; then
    cp "$IMG" "$copy"
    echo "run.sh: copied to $copy"
fi

[ "$boot" = 1 ] || exit 0

display=
[ "$headless" = 1 ] && display="-display none"
echo "run.sh: booting (Ctrl-A X quits)"
# shellcheck disable=SC2086
exec qemu-system-i386 -drive file="$IMG",format=raw,if=floppy -serial mon:stdio \
    -parallel file:src-assem/build/printer.txt $display
