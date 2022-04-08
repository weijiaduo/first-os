INSTALL and START Qemu-Windows 				Qemu-OnWindows - E.Lassauge - March 2015
==============================

INSTALL from ZIP
----------------

Choose a folder and unzip the Qemu-X.X.X-windows.zip file

The directory structure is:
Qemu-windows
├── Bios
│   └── keymaps
├── Doc
├── share
├── test-<xxx>
├── qemu-ga.exe
├── qemu-img.exe
├── qemu-io.exe
├── qemu-system-arm.exe
├── qemu-system-armw.exe
├── qemu-system-i386.exe
├── qemu-system-i386w.exe
├── qemu-system-mipsel.exe
├── qemu-system-mipselw.exe
├── qemu-system-ppc.exe
├── qemu-system-ppcw.exe
├── qemu-system-sparc.exe
├── qemu-system-sparcw.exe
├── qemu-system-x86_64.exe
├── qemu-system-x86_64w.exe
├── test-<xxx>.bat
└── <xxx>.dll

Note:
- Bios contains all Bios images from the qemu installation
- Doc contains all documents from the qemu installation + generated txt files for additional help
- test-<xxx>: see the various bat files and README.txt in test-<xxx> (may not be present)

RUN from ZIP install
--------------------

Due to the chosen directory structure Qemu must be started the following way (by giving the Bios folder name),
I suggest creating a bat file inside the top folder or a short cut with all options:

qemu-system-XXXXw.exe -L Bios <other_options>
	or
qemu-system-XXXX.exe -L Bios <other_options>

Note:
- qemu-system-XXXX.exe is a console executable
- qemu-system-XXXXw.exe is a windows executable (System emulation executables with SDL)

- Errors and output:
	- By default are redirected to 2 text files: stdout.txt and stderr.txt
	- The SDL GUI applications still can be run from a console window
	and even send stdout and stderr to that console by setting environment
	variable SDL_STDIO_REDIRECT=no.