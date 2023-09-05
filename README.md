# AsmCompilerInC
MIPS assembly emulator written in C.

• Reads one-by-one the instruction using the pc variable. 

• Extracts opcode and funct codes (if R-type) to figure out the instruction type.

• Implements the code to extract the fields of an instruction. 

• Performs appropriate changes on the registers for each instruction.

• Manipulates correctly the pc for branch operations.

• Ensures the right memory bytes are updated by sw/lw/lhu/sh.

• Uses a method to terminate the program (e.g. nop).

Code Structure
• Makefile: The build file.

• emulator.h, main.c: source code files with main and preprocessor macros.

• emulator.c: Main program that implements the task.

• ip_csm.asm, simple_add.asm, eth_mult.asm, simple_loop.asm: sample MIPS assembly code. These files should work with the MARS emulator and should be used in order to test the code.


To run the program use:

• make

• ./emulator -i [sample assembly file]

![image](https://github.com/yahnyshc/AsmCompilerInC/assets/143096926/d77f00c9-5d52-446d-81ac-4955c26a3c1c)

![image](https://github.com/yahnyshc/AsmCompilerInC/assets/143096926/16a1251b-a445-4cf9-8f95-8d0dd8d433bd)


