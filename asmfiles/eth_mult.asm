.data
fields: .word 88 77 0
.text
lui $t0 0x1001
lw $a0 0($t0)
lw $a1 4($t0)
add $v0 $zero $zero
addi $t2 $zero 1
loop: andi $t1 $a0 1
bne $t1 $t2 skipadd
add $v0 $v0 $a1
skipadd: srl $a0 $a0 1
sll $a1 $a1 1
bne $a0 $zero loop
sw $v0 8($t0)
