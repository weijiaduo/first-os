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
REM Exemple BAT script for starting qemu-system-i386.exe on windows host with a
REM small Linux disk image containing a 2.6.20 Linux kernel, X11 and various utilities
REM to test QEMU (See http://wiki.qemu.org/Testing)

REM Start qemu on windows.


REM QEMU_AUDIO_DRV=dsound or fmod or sdl or none can be used. See qemu -audio-help.
SET QEMU_AUDIO_DRV=dsound

REM SDL_AUDIODRIVER=wav or dsound can be used.
SET SDL_AUDIODRIVER=dsound

REM QEMU_AUDIO_LOG_TO_MONITOR=1 displays log messages in QEMU monitor.
SET QEMU_AUDIO_LOG_TO_MONITOR=1

REM SDL_STDIO_REDIRECT=no display stdout and stderr to console
REM Without SDL compiled in stdout and stderr go to console

REM ################################################
REM # Boot disk image
REM # Removed "-k fr" for the french keyboard: it
REM # doesn't work BUT inside guest "loadkeys /lib/kbd/fr-pc.map" works!
REM # X11, nic and soundhw were tested and are working!
REM # try madplay 20thfull.mp2 or xinit
REM # Added '-usb -usbdevice tablet' for Seamless mouse
REM # Changed disk format to qcow2 to reduce zip size
REM ################################################

START qemu-system-i386w.exe -L Bios ^
-m 128M -vga std -soundhw es1370 ^
-boot menu=on,splash=bootsplash.bmp,splash-time=2500 ^
-rtc base=localtime,clock=host -parallel none -serial none ^
-name linux-0.2 -drive file=test-i386/hda.qcow2,media=disk,cache=writeback ^
-netdev type=user,id=mynet0 -device ne2k_pci,netdev=mynet0 ^
-no-acpi -no-hpet -no-reboot -usb -usbdevice tablet
