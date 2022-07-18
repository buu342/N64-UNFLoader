# assembler directives
.set noat      # allow manual use of $at
.set noreorder # don't insert nops after branches
.set gp=64

.include "macros.inc"

.section .start, "ax"

glabel __start
	la $t0, _codeSegmentBssStart
    la $t1, _codeSegmentBssSize
bss_clear:
    addi $t1, $t1, -8
    sw $zero, ($t0)
    sw $zero, 4($t0)
    bnez $t1, bss_clear
	addi $t0, $t0, 8
    la $t2, nuBoot #Boot function address
	la $sp, nuMainStack + 0x2000 #Setup boot stack pointer, change stack size if needed here
    jr $t2
    nop