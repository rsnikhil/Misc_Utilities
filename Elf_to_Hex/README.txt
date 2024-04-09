Program to convert an ELF file into a memhex32 file:
    'make exe' => Elf_to_Memhex32.exe

Program to "reshape" a memhex32 file into memhex files for other widths,
e.g., to memhex8, memhex16, memhex512, ...
    ./Memhex32_reshape.py  --help

NOTE: Use the correct "width" memhex in your application:

  * memhex32 file:  each entry, zero-extended, represents 4 bytes (32 bits);
                    addresses increment by 4
		    '@address' is a word address

  * memhex8 file:   each entry, zero-extended, represents 1 byte (8 bits);
                    addresses increment by 1
		    '@address' is a byte address
  * memhex512 file: each entry, zero-extended, represents 64 bytes (512 bits);
                    addresses increment by 64
		    '@address' is address of 64-byte "word"
