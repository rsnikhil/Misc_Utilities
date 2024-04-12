// Copyright (c) 2013-2024 Bluespec, Inc. All Rights Reserved
// Author: Rishiyur S. Nikhil
// Some fragments taken from earlier version of Catamaran

// Program to convert an ELF file into a memhex32 file.
// Run with --help or with no command-line args for help.

// ================================================================
// Standard C includes

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <gelf.h>

// ================================================================
// Result codes returned by functions

#define RC_OK  0
#define RC_ERR 1

// ================================================================
// Features of the ELF binary

typedef struct {
    int       bitwidth;
    uint64_t  min_paddr;
    uint64_t  max_paddr;
    uint64_t  num_bytes_total;

    uint64_t  pc_start;       // Addr of label  '_start'
    uint64_t  pc_exit;        // Addr of label  'exit'
    uint64_t  tohost_addr;    // Addr of label  'tohost'
} Elf_Features;

static const char start_symbol  [] = "_start";
static const char exit_symbol   [] = "exit";
static const char tohost_symbol [] = "tohost";

// ================================================================

// From /usr/include/gelf.h
//    typedef Elf64_Phdr GElf_Phdr;

// From /usr/include/elf.h
//    typedef struct
//    {
//      Elf64_Word    p_type;                 /* Segment type */
//      Elf64_Word    p_flags;                /* Segment flags */
//      Elf64_Off     p_offset;               /* Segment file offset */
//      Elf64_Addr    p_vaddr;                /* Segment virtual address */
//      Elf64_Addr    p_paddr;                /* Segment physical address */
//      Elf64_Xword   p_filesz;               /* Segment size in file */
//      Elf64_Xword   p_memsz;                /* Segment size in memory */
//      Elf64_Xword   p_align;                /* Segment alignment */
//    } Elf64_Phdr;

static
uint64_t fn_vaddr_to_paddr (Elf *e, uint64_t vaddr, uint64_t size)
{
    GElf_Phdr phdr;    // = Elf64_Phdr
    int index = 0;

    /*
    fprintf (stdout, "%16s", "Virtual address");
    fprintf (stdout, " %16s", "Virt addr lim");
    fprintf (stdout, "    ");
    fprintf (stdout, " %9s", "Type");
    fprintf (stdout, " %5s", "Flags");
    fprintf (stdout, " %16s", "File offset");
    fprintf (stdout, " %8s", "Phy addr");
    fprintf (stdout, " %8s", "Sz file");
    fprintf (stdout, " %8s", "Sz mem");
    fprintf (stdout, " %8s", "Alignment\n");
    */

    while (gelf_getphdr (e, index, & phdr) != NULL) {
	/*
	fprintf (stdout, "%016lx",  phdr.p_vaddr);
	fprintf (stdout, " %016lx",  phdr.p_vaddr + phdr.p_memsz);

	fprintf (stdout, " [%02d]", index);
	fprintf (stdout, " %8x",    phdr.p_type);
	fprintf (stdout, " %5x",    phdr.p_flags);
	fprintf (stdout, " %16lx",  phdr.p_offset);
	fprintf (stdout, " %8lx",   phdr.p_paddr);
	fprintf (stdout, " %8lx",   phdr.p_filesz);
	fprintf (stdout, " %8lx",   phdr.p_memsz);
	fprintf (stdout, " %8lx\n", phdr.p_align);
	*/

	if ((phdr.p_vaddr <= vaddr) && (size <= phdr.p_memsz)) {
	    return (vaddr - phdr.p_vaddr) + phdr.p_paddr;
	}
	index++;
    }
    // Did not find segment for this.
    fprintf (stdout,
	     "ERROR: %s: Could not find segment containing given virtual range\n",
	     __FUNCTION__);
    fprintf (stdout, "    vaddr %0" PRIx64 "  size %0" PRIx64 "\n", vaddr, size);
    exit (1);
}

// ================================================================
// Store-buffer
// scan_elf() emits as stream of (addr, byte) into this store-buffer.
// It buffers up to four bytes, 4-byte aligned
// It writes out the buffer when addr is for a different word than buf_addr.

static int64_t   last_addr_written = -8;
static int64_t   buf_addr = -8;
static uint32_t  buf_data;
static uint8_t  *p = NULL;

static
void emit (FILE *fp_out, int64_t addr, uint8_t byte)
{
    p = (uint8_t *) (& buf_data);

    if (addr < buf_addr) {
	fprintf (stdout, "WARNING: addr %08" PRIx64 " is < last addr %08" PRIx64 "\n",
		 addr, buf_addr);
	// exit (1);
    }

    int64_t addr_aligned = ((addr >> 2) << 2);
    uint8_t lsbs         = (addr & 0x3);

    if (buf_addr < 0) buf_addr = addr_aligned;    // On first invocation

    if (buf_addr != addr_aligned) {
	// Write out the buffer
	// Write out addr line if needed
	if ((last_addr_written + 4) != buf_addr) {
	    fprintf (fp_out, "@%0" PRIx64 "    // ---- %0" PRIx64 "\n",
		     buf_addr >> 2, buf_addr);
	}
	// Write out data
	// fprintf (fp_out, "%08x\n", buf_data);
	fprintf (fp_out, "%08x     // %08" PRIx64 "\n", buf_data, buf_addr);
	last_addr_written = buf_addr;

	buf_addr = addr_aligned;
	buf_data = 0;
    }

    if (! (buf_addr == addr_aligned)) {
	fprintf (stdout, "ERROR: %s: buf_addr:%08" PRIx64 "  addr_aligned %08" PRIx64 "\n",
		 __FUNCTION__, buf_addr, addr_aligned);
	assert (buf_addr == addr_aligned);
    }

    // Merge-in the new byte
    // fprintf (stdout, "DEBUG: p[%0d] = %02x\n", lsbs, byte);
    p [lsbs] = byte;
}

static
void flush_store_buffer (FILE *fp_out)
{
    if (buf_addr >= 0) {
	// Write out the buffer
	// Write out addr line if needed
	if (buf_addr != last_addr_written) {
	    fprintf (fp_out, "@%0" PRIx64 "    // ---- %0" PRIx64 "\n",
		     buf_addr >> 2, buf_addr);
	}
	// Write out data
	fprintf (fp_out, "%08x  // %08" PRIx64 "\n", buf_data, buf_addr);
	last_addr_written = buf_addr;
    }
}

// ================================================================
// Internal function to scan the ELF file and write memhex32
// Returns true on success, false otherwise.

static
bool scan_elf (Elf              *e,
	       const GElf_Ehdr  *ehdr,
	       Elf_Features     *p_features,
	       FILE             *fp_log,
	       const int         verbosity,
	       FILE             *fp_out)
{
    // Grab the string section index
    size_t shstrndx;
    shstrndx = ehdr->e_shstrndx;

    // Iterate through each of the sections looking for code that should be loaded
    Elf_Scn  *scn   = 0;
    GElf_Shdr shdr;

    while ((scn = elf_nextscn (e,scn)) != NULL) {

        // get the header information for this section
        gelf_getshdr (scn, & shdr);

	char *sec_name = elf_strptr (e, shstrndx, shdr.sh_name);
	if ((verbosity != 0) && (fp_log != NULL))
	    fprintf (fp_log, "  %-20s:", sec_name);

	Elf_Data *data = 0;
	// 'ALLOC' type sections are candidates to be loaded
	if (shdr.sh_flags & SHF_ALLOC) {
	    data = elf_getdata (scn, data);

	    // data->sh_addr may be virtual; find the phys addr from the segment table
	    uint64_t section_paddr = fn_vaddr_to_paddr (e, shdr.sh_addr, data->d_size);
	    if ((verbosity != 0) && (fp_log != NULL)) {
#if !defined(__APPLE__)
		// Linux
		fprintf (fp_log, " vaddr %10" PRIx64 " to vaddr %10" PRIx64 ";",
			 shdr.sh_addr,
			 shdr.sh_addr + data->d_size);
#else
		// MacOS
		fprintf (fp_log, " vaddr %10lx to vaddr %10lx;",
			 shdr.sh_addr,
			 shdr.sh_addr + data->d_size);
#endif
		fprintf (fp_log, " size 0x%lx (=%0ld)\n",
			 data->d_size,
			 data->d_size);
		fprintf (fp_log, "                        paddr %10" PRIx64 "\n",
			 section_paddr);
	    }

	    // Record some features
	    if (section_paddr < p_features->min_paddr)
		p_features->min_paddr = section_paddr;
	    if (p_features->max_paddr < (section_paddr + data->d_size - 1))
		p_features->max_paddr = section_paddr + data->d_size - 1;
	    p_features->num_bytes_total += data->d_size;

	    if (data->d_size == 0) {
		// Section is empty
		if ((verbosity != 0) && (fp_log != NULL))
		    fprintf (fp_log, "    Empty section (0-byte size), ignoring\n");
	    }
	    else {
		uint64_t addr   = section_paddr;
		uint64_t size   = data->d_size;
		uint8_t *buf    = data->d_buf;

		if (shdr.sh_type != SHT_NOBITS) {
		    // Normal section
		    if (fp_log != NULL)
			fprintf (fp_log,
				 "    Section addr 0x%0" PRIx64 ", size 0x%0" PRIx64 "\n",
				 addr, size);
		    if ((addr & 0x3) != 0) {
			fprintf (fp_log, "WARNING: section addr is not 4-byte aligned\n");
		    }
		    for (uint64_t j = 0; j < size; j++)
			emit (fp_out, addr + j, buf [j]);
		}
		else if ((strcmp (sec_name, ".bss") == 0)
			 || (strcmp (sec_name, ".sbss") == 0)) {
		    // Section to be zeroed
		    if (fp_log != NULL) {
			fprintf (fp_log, "    Section .bss/.sbss:");
			fprintf (fp_log, " addr:%0" PRIx64, addr);
			fprintf (fp_log, " size:%0" PRIx64 "\n", size);
		    }
		    if ((addr & 0x3) != 0) {
			fprintf (fp_log, "WARNING: section addr is not 4-byte aligned\n");
		    }
		    for (uint64_t j = 0; j < size; j+= 4) {
			emit (fp_out, addr + j, 0);
		    }
		}
		else {
		    if ((verbosity != 0) && (fp_log != NULL))
			fprintf (fp_log, "    No bits to load\n");
		}
	    }
	}
	// If we find the symbol table, search for symbols of interest
	else if (shdr.sh_type == SHT_SYMTAB) {
	    if ((verbosity != 0) && (fp_log != NULL))
		fprintf (fp_log, "\n    Search for symbols  '%s'  '%s'  '%s'\n",
			 start_symbol, exit_symbol, tohost_symbol);

 	    // Get the section data
	    data = elf_getdata (scn, data);

	    // Get the number of symbols in this section
	    int symbols = shdr.sh_size / shdr.sh_entsize;

	    // search for the uart_default symbols we need to potentially modify.
	    GElf_Sym sym;
	    int i;
	    for (i = 0; i < symbols; ++i) {
	        // get the symbol data
	        gelf_getsym (data, i, &sym);

		// get the name of the symbol
		char *name = elf_strptr (e, shdr.sh_link, sym.st_name);

		// Look for, and remember PC of the start symbol
		if (strcmp (name, start_symbol) == 0) {
		    p_features->pc_start = fn_vaddr_to_paddr (e, sym.st_value, 4);
		}
		// Look for, and remember PC of the exit symbol
		else if (strcmp (name, exit_symbol) == 0) {
		    p_features->pc_exit = fn_vaddr_to_paddr (e, sym.st_value, 4);
		}
		// Look for, and remember addr of 'tohost' symbol
		else if (strcmp (name, tohost_symbol) == 0) {
		    // tohost usually is in MMIO space, won't have a virtual address
		    // p_features->tohost_addr = fn_vaddr_to_paddr (e, sym.st_value, 4);
		    p_features->tohost_addr = sym.st_value;
		}
	    }

	    if ((verbosity != 0) && (fp_log != NULL)) {
		fprintf (fp_log, "    _start");
		if (p_features->pc_start == -1)
		    fprintf (fp_log, "    Not found\n");
		else
		    fprintf (fp_log, "    %0" PRIx64 "\n", p_features->pc_start);

		fprintf (fp_log, "    exit  ");
		if (p_features->pc_exit == -1)
		    fprintf (fp_log, "    Not found\n");
		else
		    fprintf (fp_log, "    %0" PRIx64 "\n", p_features->pc_exit);

		fprintf (fp_log, "    tohost");
		if (p_features->tohost_addr == -1)
		    fprintf (fp_log, "    Not found\n");
		else
		    fprintf (fp_log, "    %0" PRIx64 "\n", p_features->tohost_addr);
	    }
	}

	else {
	    if ((verbosity != 0) && (fp_log != NULL))
		fprintf (fp_log, " ELF section ignored\n");
	}
    }
    flush_store_buffer (fp_log);

    return RC_OK;
}

// ================================================================
// This is the main call for the conversion
// Returns RC_OK on success, RC_ERR otherwise.

int convert_ELF (const char     *elf_filename,
		 const char     *memhex32_filename,
		 FILE           *fp_log,
		 const int       verbosity)
{
    Elf_Features  elf_features;
    Elf          *e;
    int           rc;

    // Verify the elf library version
    if (elf_version (EV_CURRENT) == EV_NONE) {
	fprintf (stdout,
		 "ERROR: %s: Failed to initialize the libelf library.\n",
		 __FUNCTION__);
	rc = RC_ERR;
	goto done2;
    }

    if (fp_log != NULL)
	fprintf (fp_log, "Convert ELF file: %s\n", elf_filename);

    // Open the ELF file for reading
    int fd = open (elf_filename, O_RDONLY, 0);
    if (fd < 0) {
	fprintf (stdout,
		 "ERROR: could not open ELF file: %s\n",
		 elf_filename);
	rc = RC_ERR;
	goto done2;
    }

    // Open the Memhex32 file for writing
    FILE *fp_out = fopen (memhex32_filename, "w");
    if (fp_out == NULL) {
	fprintf (stdout,
		 "ERROR: could not open Memhex32 file: %s\n",
		 memhex32_filename);
	rc = RC_ERR;
	goto done2;
    }

    // Initialize the Elf object with the open file
    e = elf_begin (fd, ELF_C_READ, NULL);
    if (e == NULL) {
	fprintf (stdout, "ERROR: %s: elf_begin() initialization failed!\n",
		 __FUNCTION__);
	rc = RC_ERR;
	goto done;
    }

    // Verify that the file is an ELF file
    if (elf_kind (e) != ELF_K_ELF) {
	fprintf (stdout, "  '%s' is not an ELF file\n", elf_filename);
        elf_end (e);
	rc = RC_ERR;
	goto done;
    }

    // Get the ELF header
    GElf_Ehdr ehdr;
    if (gelf_getehdr (e, & ehdr) == NULL) {
	fprintf (stdout, "ERROR: %s: get_getehdr() failed: %s\n",
		 __FUNCTION__, elf_errmsg (-1));
        elf_end (e);
	rc = RC_ERR;
	goto done;
    }

    // Is this a 32b or 64 ELF?
    if (gelf_getclass (e) == ELFCLASS32) {
	if (verbosity != 0)
	    fprintf (fp_log, "  This is a 32-bit ELF file\n");
	elf_features.bitwidth = 32;
    }
    else if (gelf_getclass (e) == ELFCLASS64) {
	if (verbosity != 0)
	    fprintf (fp_log, "  This is a 64-bit ELF file\n");
	elf_features.bitwidth = 64;
    }
    else {
	fprintf (stdout, "ERROR: ELF file '%s' is not 32b or 64b\n",
		 elf_filename);
        elf_end (e);
	rc = RC_ERR;
	goto done;
    }

    // ----------------
    // Verify ELF is for RISC-V (e_machine = 0xF3 = 243)
    // https://github.com/riscv-non-isa/riscv-elf-psabi-doc/blob/master/riscv-elf.adoc
    // http://www.sco.com/developers/gabi/latest/ch4.eheader.html
#ifndef EM_RISCV
    // This is for elf lib on MacOS (Ventura 13.1, 2023-01-31) where EM_RISCV is not defined
#define EM_RISCV 0xF3
#endif
    if (ehdr.e_machine != EM_RISCV) {
	fprintf (stdout,
		 "ERROR: %s: %s is not a RISC-V ELF file?\n",
		 __FUNCTION__, elf_filename);
        elf_end (e);
	rc = RC_ERR;
	goto done;
    }

    // ----------------
    // Verify we are dealing with a little endian ELF
    if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB) {
	fprintf (stdout,
		 "ERROR: %s is big-endian, not supported\n",
		 elf_filename);
        elf_end (e);
	rc = RC_ERR;
	goto done;
    }

    // ----------------------------------------------------------------
    // Ok, all checks done, ready to read the ELF and convert it.

    elf_features.bitwidth        = 0;
    elf_features.min_paddr       = 0xFFFFFFFFFFFFFFFFllu;
    elf_features.max_paddr       = 0x0000000000000000llu;
    elf_features.num_bytes_total = 0;
    elf_features.pc_start        = 0xFFFFFFFFFFFFFFFFllu;
    elf_features.pc_exit         = 0xFFFFFFFFFFFFFFFFllu;
    elf_features.tohost_addr     = 0xFFFFFFFFFFFFFFFFllu;

    // ----------------------------------------------------------------
    // Extract ELF payload and write to target mem, and record features

    rc = scan_elf (e, & ehdr, & elf_features,
		   fp_log, verbosity, fp_out);
    if (rc != RC_OK) {
	goto done;
    }

    // Report features
    if ((verbosity != 0) && (fp_log != NULL)) {
	uint64_t span = elf_features.max_paddr + 1 - elf_features.min_paddr;
	fprintf (fp_log,
		 "    Size: 0x%0" PRIx64 " (%0" PRId64 ") bytes\n",
		 elf_features.num_bytes_total, elf_features.num_bytes_total);
	fprintf (fp_log,
		 "    Min paddr: %10" PRIx64 "\n", elf_features.min_paddr);
	fprintf (fp_log,
		 "    Max paddr: %10" PRIx64 "\n", elf_features.max_paddr);
	fprintf (fp_log,
		 "    Span:      %10" PRIx64 " (=%0" PRId64 ") bytes\n",
		 span, span);
    }

    // ----------------------------------------------------------------

    // Close elf object
    elf_end (e);

    rc = RC_OK;

 done:
    if (fd >= 0) close (fd);
 done2:
    return rc;
}

// ================================================================

void print_usage (int argc, char *argv[])
{
    fprintf (stdout, "Usage:\n");
    fprintf (stdout, "  %s  <filename.elf (input)>  <filename.memhex32 (output)>\n", argv [0]);
    fprintf (stdout, "  Converts ELF file into Memhex32 file\n");
}

int main (int argc, char *argv[])
{
    if ((argc <= 1)
	|| (argc != 3)
	|| (strcmp (argv [0], "-h") == 0)
	|| (strcmp (argv [1], "--help") == 0)) {
	print_usage (argc, argv);
	return 0;
    }

    int verbosity = 1;
    int rc = convert_ELF (argv [1], argv [2], stdout, verbosity);

    return rc;
}

// ================================================================
