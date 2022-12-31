Generate SoC Map

This program generates code for an "address map" for an SoC,
taking a textual specification as an input.

Each line in the specification file can be:

    blank    (ignored)
    VAL  <name>  <value>
    IO   <name>  <base addr>  <size>
    MEM  <name>  <base addr>  <size>
    ... // comment through end of line

See 'Soc_Map_Spec_Catamaran.txt' for an example.

Then, use a generator to generate code in a particular language for
this SoC Map:

    $ ./Gen_Soc_Map_BSV.py    --help
    $ ./Gen_Soc_Map_BSV.py    <spec file (input)>    <code file (output)>

[Currently there is only one generator, for BSV code]

// ================
Example:

    $ ./Gen_Soc_Map_BSV.py    SoC_Map_Spec_Catamaran.txt  SoC_Map_Catamaran.bsv

The Catamaran Spec is used in:
    Bluespec Flute RISC-V CPU
        (https:~/GitHub/Bluespec/Flute)
    Bluespec Catamaran RISC-V development environment
        (https://github.com/bluespec/AWSteria_RISCV_Virtio)
    and more.

// ================
