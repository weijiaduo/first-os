@ECHO OFF
REM ##############################################################################
REM (c) Eric Lassauge - December 2015
REM <lassauge {AT} users {DOT} sourceforge {DOT} net >
REM
REM ##############################################################################
REM    This program is free software: you can redistribute it and/or modify
REM    it under the terms of the GNU General Public License as published by
REM    the Free Software Foundation, either version 3 of the License, or
REM    (at your option) any later version.
REM
REM    This program is distributed in the hope that it will be useful,
REM    but WITHOUT ANY WARRANTY; without even the implied warranty of
REM    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
REM    GNU General Public License for more details.
REM
REM    You should have received a copy of the GNU General Public License
REM    along with this program.  If not, see <http://www.gnu.org/licenses/>
REM ##############################################################################
REM Exemple BAT script for starting qemu-system-sparc.exe on windows host with a
REM SPARC Linux 2.6 test kernel and initrd disk image
REM to test QEMU (See http://wiki.qemu.org/Testing)

REM Start qemu on windows.


REM QEMU_AUDIO_DRV=dsound or wav or none can be used. See qemu -audio-help.
SET QEMU_AUDIO_DRV=dsound


REM QEMU_AUDIO_LOG_TO_MONITOR=1 displays log messages in QEMU monitor.
SET QEMU_AUDIO_LOG_TO_MONITOR=1

REM ################################################
REM # Boot kernel
REM ################################################
START qemu-system-sparcw.exe -L Bios -m 128M -k fr ^
-name SPARC -kernel test-sparc/vmlinux-2.6.11+tcx -initrd test-sparc/linux.img ^
-append "root=/dev/ram"