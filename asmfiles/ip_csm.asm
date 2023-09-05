.data
packet: .word 0x45000073 0x00004000 0x40110000 0xc0a80001 0xc0a800c7
.text
lui $s0 0x1001
lui $t0 0x1001
addi $t0 $t0 20
addi $t1 $t1 0
loop: lhu $a0 0($s0)
add $t1 $t1 $a0
addi $s0 $s0 2
bne $s0 $t0 loop
srl $t0 $t1 16
andi $t1 $t1 0xFFFF
add $t1 $t1 $t0
lui $s0 0x1001
sh $t1 8($s0)
nop
