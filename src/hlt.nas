[BITS 32]
		MOV		AL,'A'
		CALL    2*8:0xbeb
fin:
		HLT
		JMP 	fin
