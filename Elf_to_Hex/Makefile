.PHONY: help
help:
	@echo "Targets:"
	@echo "  exe, Elf_to_Hex32.exe    Compile C source file(s) to create executable"
	@echo ""
	@echo "  clean                    Delete temporary/intermediate files"
	@echo "  full_clean               Restore dir to pristine state"

EXE=Elf_to_Memhex32.exe

.PHONY: exe
exe: $(EXE)

# ****************************************************************
# For Apple MacOS

# Prerequisit:
#    $ brew install libelf
#    $ brew install aarch64-elf-gcc    (not sure if this is necessary)

ifeq ($(shell uname -s),Darwin)

# For include <gelf.h>, after 'brew install libelf'
CFLAGS += -I /opt/homebrew/include
CFLAGS += -I /opt/homebrew/include/libelf/

# For libelf.a
LDFLAGS += -L /opt/homebrew/lib

endif

# ****************************************************************
# Compile-and-link all to executable

CFLAGS += -Wall
LDLIBS = -lelf -lm

$(EXE): Elf_to_Memhex32.c
	$(CC)  $(CFLAGS) -o  $(EXE)  Elf_to_Memhex32.c  $(LDFLAGS) $(LDLIBS)
	@echo "INFO: Executable $(EXE) created. Run it with --help for options."

# ****************************************************************

.PHONY: clean
clean:
	rm -r -f  *~

.PHONY: full_clean
full_clean: clean
	rm -f  *~  *.exe
