Fetch the i386 image from http://wiki.qemu.org/download/linux-0.2.img.bz2
Convert to qcow2:
	bunzip2 linux-0.2.img.bz2
	../qemu-img.exe convert -O qcow2 linux-0.2.img hda.qcow2
	rm linux-0.2.img
