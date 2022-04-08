#!/bin/sh

MACH="-M virtex-ml507"

# Select your QEMU binary.
QEMU=~/src/c/qemu/git/build-qemu/ppc-softmmu/qemu-system-ppc
#QEMU=qemu-system-ppc

KERNEL="-kernel vmlinux" 

$QEMU $MACH $BOOTROM $NIC0 $KERNEL -m 256 -append "rdinit=/boot.sh console=ttyS0" -nographic $*
