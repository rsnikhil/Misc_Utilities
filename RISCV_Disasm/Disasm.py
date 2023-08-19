#!/usr/bin/python3

# A 'main' program for RISC-V instruction disassembly
# Copyright (c) 2020-2023 Rishiyur S. Nikhil and Bluespec, Inc., All Rights Reserved
# Author: Rishiyur Nikhil

# TODO (2020-10-07):
#  - Command-line args to restrict ISA options

import sys
import fileinput

import disasm_lib

# ================================================================

def print_usage (argv):
    sys.stdout.write ("Usage:\n")
    sys.stdout.write ("    {:s}  <arg> ... <arg>\n".format (argv [0]))
    sys.stdout.write ("  Args:\n")
    sys.stdout.write ("    -h, --help      Print this help and exit\n")
    sys.stdout.write ("    <hex number>    Disassembles this as a RISC-V instruction\n")
    sys.stdout.write ("    <filename>      Disassembles each line in file as a RISC-V instruction.\n")
    sys.stdout.write ("                    (Each line in file should be a hex number.)\n")
    sys.stdout.write ("    --RV32,--RV64,--RV128\n")
    sys.stdout.write ("                    Context for subsequent command-line args (initial RV64).\n")
    sys.stdout.write ("                    Affects decoding of shift and compressed instrs.\n")
    sys.stdout.write ("                    Can be used multiple times.\n")
    sys.stdout.write ("  All output is on stdout.\n");

# ================================================================

def main (argv = None):
    if (len (argv) == 1) or ("-h" in argv) or ('--help' in argv):
        print_usage (argv)
        return 0

    argv = argv [1:]

    sys.stdout.write ("Default RV64\n")
    xlen = 64

    for arg in argv:
        if (arg.upper() == "--RV32"):
            xlen = 32
            sys.stdout.write ("INFO: RV32\n")
        elif (arg.upper() == "--RV64"):
            xlen = 64
            sys.stdout.write ("INFO: RV64\n")
        elif (arg.upper() == "--RV128"):
            xlen = 128
            sys.stdout.write ("INFO: RV128\n")
        else:
            try:
                instr = int (arg, 16)    # will raise exception if not parseable as a hex number
                process_line ("Command-line arg", xlen, arg)
            except:
                process_pre (arg)
                for line in fileinput.input (arg):
                    process_line (arg, xlen, line)
                process_post (arg)
    return 0

# ================================================================
# Process the input file

num_input_lines = 0

def process_pre (filename):
    global num_input_lines
    num_input_lines = 0
    sys.stdout.write ("File start: '{:s}'\n".format (filename))

def process_line (filename, xlen, line):
    global num_input_lines
    num_input_lines = num_input_lines + 1
    line = line.rstrip ()

    try:
        # Only look at the first non-whitespace "word" in the line, if any
        words = line.split ()
        if (words == []):
            # empty line; ignore
            sys.stdout.write ("L{:d}:\n".format (num_input_lines))
            return
        instr = int (words [0], 16)
    except:
        return

    s = disasm_lib.disasm (xlen, instr)
    sys.stdout.write ("L{:d}: {:09_x}  {:s}\n".format (num_input_lines, instr, s))

def process_post (filename):
    global num_input_lines
    sys.stdout.write ("File end: '{:s}'  line count {:d}\n".format (filename, num_input_lines))

# ================================================================
# For non-interactive invocations, call main() and use its return value
# as the exit code.
if __name__ == '__main__':
  sys.exit (main (sys.argv))
