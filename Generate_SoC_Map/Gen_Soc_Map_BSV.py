#!/usr/bin/python3

# Copyright (c) 2022 Rishiyur S. Nikhil

import sys
import os

# ================================================================

def main (argv = None):
    if ("--help" in argv) or ("-h" in argv) or (len (argv) != 3) :
        sys.stdout.write ("Usage:  {:s}  <in_file>  <out_file>\n".format (argv [0]))
        sys.stdout.write ("    <in_file>    SoC Map, usually .txt\n")
        sys.stdout.write ("    <out_file>   BSV file, usually .bsv\n")
        return 0

    in_filename = argv [1]
    out_filename = argv [2]

    #----------------
    # Read and parse input file
    sys.stdout.write ("  Reading input file:  {:s}\n".format (in_filename))

    with open (in_filename, "r") as f_in:
        (constants, regions) = read_input_file (f_in)

    #----------------
    # Show parsed input

    if True:
        sys.stdout.write ("  ----------------\n")
        sys.stdout.write ("  Constants\n")
        for c in constants:
            sys.stdout.write ("    {:s}  {:_x}\n".format (c ["name"], c ["val"]))

        sys.stdout.write ("  Regions\n")
        for r in regions:
            sys.stdout.write ("    {:3s}  {:#011_x}  {:#011_x}  {:s}\n"
                              .format (r ["type"],
                                       r ["base"],
                                       r ["size"],
                                       r ["name"]))
        sys.stdout.write ("  ----------------\n")

    #----------------
    # Generate output text

    lines = construct_output (constants, regions)

    #----------------
    # Write output text to output file
    sys.stdout.write ("  Writing output file: {:s}\n".format (out_filename))
    with open (out_filename, "w") as f_out:
        for line in lines:
            f_out.write (line)
            f_out.write ("\n")

    return 0

# ================================================================
# read_input_file ()
# Two-element lines are constant declarations (e.g., pc_reset_value <value>)
# Three-element lines are address-regions     (e.g., dram  <addr>  <size>
# All values interpreted in hexadecimal

def read_input_file (f_in):

    constants = []
    regions   = []

    linenum = 0
    while True:
        original_line = f_in.readline()
        if (original_line == ""): break
        linenum += 1

        line = original_line.strip()

        line = line.rsplit ("//", 1) [0]    # Remove end-of-line comments

        if (line == ""): continue    # Skip blank lines

        words = line.split()

        if (len (words) == 3) and (words [0].upper() == "VAL"):
            constants.append ( {"name" : words [1],
                                "val"  : int (words [2], 16)} )

        elif ((len (words) == 4)
              and ((words [0].upper() == "IO") or (words [0].upper() == "MEM"))):
            regions.append ( {"type":  words [0],
                              "name" : words [1],
                              "base" : int (words [2], 16),
                              "size" : int (words [3], 16)} )

        else:
            sys.stdout.write ("Ignoring this line: unrecognized format\n")
            sys.stdout.write ("  L{:d}: {:s}\n".format (linenum, original_line))

    return (constants, regions)

# ================================================================
# contruct_output ()
# Returns a list of lines (strings without linebreaks)

def construct_output (constants, regions):

    lines = [
        "// THIS IS A PROGRAM-GENERATED FILE; DO NOT EDIT!",
        "// See: https://github.com/rsnikhil/Misc_Utilities    /Generate_SoC_Map",
        "",
        "package SoC_Map;",
        "",
        "// ================================================================",
        "// This module defines the overall 'address map' of the SoC, showing",
        "// the addresses serviced by each server IP, and which addresses are",
        "// memory vs. I/O., etc.",
        "",
        "// ***** WARNING! WARNING! WARNING! *****",
        "",
        "// During system integration, this address map should be identical to",
        "// the system interconnect settings (e.g., routing of requests between",
        "// clients and servers).  This map is also needed by software so that",
        "// it knows how to address various IPs.",
        "",
        "// This module contains no state; it just has constants, and so can be",
        "// freely instantiated at multiple places in the SoC module hierarchy",
        "// at no hardware cost.  It allows this map to be defined in one",
        "// place and shared across the SoC.",
        "",
        "// ================================================================",
        "// Exports",
        "",
        "export  SoC_Map_IFC (..), mkSoC_Map;",
        "",
        "// ================================================================",
        "// Bluespec library imports",
        "",
        "// None",
        "",
        "// ================================================================",
        "// Project imports",
        "",
        "import Fabric_Defs :: *;    // Only for type Fabric_Addr",
    ]

    # ================================================================
    # Interface definition

    lines.extend ([
        "",
        "// ================================================================",
        "// Interface for the address map module",
        "",
        "interface SoC_Map_IFC;",
    ])

    for r in regions:
        lines.extend (["   // ---------------- {:s} region".format (r ["type"]),
                       "   (* always_ready *) method Fabric_Addr  m_{:s}_addr_base;".format (r ["name"]),
                       "   (* always_ready *) method Fabric_Addr  m_{:s}_addr_size;".format (r ["name"]),
                       "   (* always_ready *) method Fabric_Addr  m_{:s}_addr_lim;" .format (r ["name"]),
                       ""])

    lines.extend (["   // ---------------- Predicates ----------------",
                   "   (* always_ready *)",
                   "   method  Bool  m_is_mem_addr (Fabric_Addr addr);",
                   "",
                   "   (* always_ready *)",
                   "   method  Bool  m_is_IO_addr (Fabric_Addr addr);",
                   ""])

    lines.extend (["   // ---------------- Constants ----------------"])
    for c in constants:
        lines.extend (["   (* always_ready *)  method  Bit #(64)  m_{:s};".format (c ["name"])])

    lines.extend (["endinterface"])

    # ================================================================
    # Module definition

    lines.extend ([
        "",
        "// ================================================================",
        "// The address map module",
        "",
        "module mkSoC_Map (SoC_Map_IFC);",
        ""])

    # ----------------------------------------------------------------
    # Region definitions

    for r in regions:
        lines. extend ([
            "   // ---------------- {:s} region".format (r ["type"]),
            "   Fabric_Addr {:s}_addr_base = 'h_{:09_x};".format (r ["name"], r ["base"]),
            "   Fabric_Addr {:s}_addr_size = 'h_{:09_x};".format (r ["name"], r ["size"]),
            "   Fabric_Addr {:s}_addr_lim  = {:s}_addr_base + {:s}_addr_size;"
            .format (r ["name"], r ["name"], r ["name"]),
            "",
            "   function Bool fn_is_{:s}_addr (Fabric_Addr addr);".format (r ["name"]),
            "      return (({:s}_addr_base <= addr) && (addr < {:s}_addr_lim));"
            .format (r ["name"], r ["name"], r ["name"]),
            "   endfunction",
            ""])

    # ----------------------------------------------------------------
    # Memory address predicate

    mem_regions = [ r for r in regions if (r ["type"] == "MEM")]
    lines.extend ([
        "   // ----------------------------------------------------------------",
        "   // Memory address predicate",
        "",
        "   function Bool fn_is_mem_addr (Fabric_Addr addr);"])

    lines.extend (["      return (   False"])
    for region in mem_regions:
        lines.extend (["              || fn_is_{:s}_addr (addr)".format (region ["name"] )])
    lines.extend ([
        "      );",
        "   endfunction",
        ""])

    # ----------------------------------------------------------------
    # MMIO address predicate

    io_regions = [ r for r in regions if (r ["type"] == "IO")]
    lines.extend ([
        "   // ----------------------------------------------------------------",
        "   // IO address predicate",
        "",
        "   function Bool fn_is_IO_addr (Fabric_Addr addr);"])

    lines.extend (["      return (   False"])
    for region in io_regions:
        lines.extend (["              || fn_is_{:s}_addr (addr)".format (region ["name"] )])
    lines.extend ([
        "      );",
        "   endfunction",
        ""])

    # ----------------------------------------------------------------
    # Constant defs

    lines.extend ([
        "   // ----------------------------------------------------------------",
        "   // Constants",
        ""])
    for c in constants:
        lines.extend (["   Bit #(64) {:s} = 'h_{:09_x};".format (c ["name"], c ["val"])]);
    lines.extend ([""])

    # ----------------------------------------------------------------
    # Module interface

    lines.extend ([
        "   // ----------------------------------------------------------------",
        "   // INTERFACE",
        ""])

    for r in regions:
        lines.extend ([
            "   method Fabric_Addr m_{:s}_addr_base = {:s}_addr_base;".format (r ["name"], r ["name"]),
            "   method Fabric_Addr m_{:s}_addr_size = {:s}_addr_size;".format (r ["name"], r ["name"]),
            "   method Fabric_Addr m_{:s}_addr_lim  = {:s}_addr_lim;" .format (r ["name"], r ["name"]),
            ""]);

    lines.extend ([
        "   method  Bool  m_is_mem_addr (Fabric_Addr addr) = fn_is_mem_addr (addr);",
        "   method  Bool  m_is_IO_addr (Fabric_Addr addr) = fn_is_IO_addr (addr);",
        ""])

    for c in constants:
        lines.extend (["   method  Bit #(64)  m_{:s} = {:s};".format (c ["name"], c ["name"])])

    lines.extend ([
        "endmodule",
        "",
        "// ================================================================",
        "",
        "endpackage"])

    return lines

# ================================================================
# For non-interactive invocations, call main() and use its return value
# as the exit code.

if __name__ == '__main__':
  sys.exit (main (sys.argv))
