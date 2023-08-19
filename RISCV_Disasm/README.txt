Ad-hoc RISC-V disassembler, for individual instructions, or a file of
instructions, expressed in hex.

Example usage:

    $ Disasm.py ff010113
    Default RV64
    L1: ff01_0113  ADDI sp(x2) := sp(x2), ff0    (class_ALU_I)


    $ Disasm.py instrs.txt 
    Default RV64
    File start: 'instrs.txt'
    L1: ff01_0113  ADDI sp(x2) := sp(x2), ff0    (class_ALU_I)
    L2: 0000_0593  ADDI a1(x11) := zero(x0), 0    (class_ALU_I)
    L3: 0081_3023  SD MEM [sp(x2) + 8] := s0/fp(x8)    (class_STORE)
    L4: 0011_3423  SD MEM [sp(x2) + 1] := ra(x1)    (class_STORE)
    L5: 0005_0413  ADDI s0/fp(x8) := a0(x10), 0    (class_ALU_I)
    L6: 4ed0_00ef  JAL PC+cec; ra(x1) := PC    (class_JAL)
    L7: 7501_b503  LD a0(x10) := MEM [gp(x3) + 750]    (class_LOAD)
    L8: 0585_3783  LD a5(x15) := MEM [a0(x10) + 58]    (class_LOAD)
    L9: 0007_8463  BEQ a5(x15) zero(x0); PC+8    (class_BRANCH)
    L10: 0007_80e7  JALR PC+a5(x15)+0; ra(x1) := PC    (class_JALR)
    L11: 0004_0513  ADDI a0(x10) := s0/fp(x8), 0    (class_ALU_I)
    L12: 3610_30ef  JAL PC+3b60; ra(x1) := PC    (class_JAL)
    File end: 'instrs.txt'  line count 12
