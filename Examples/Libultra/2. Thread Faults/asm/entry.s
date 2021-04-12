# assembler directives
.set noat      # allow manual use of $at
.set noreorder # don't insert nops after branches
.set gp=64

.include "macros.inc"

.section .text, "ax"

glabel entry_point
    la $t0, _codeSegmentBssStart
    la $t1, _codeSegmentBssSize
.L80246010:
    addi  $t1, $t1, -8
    sw    $zero, ($t0)
    sw    $zero, 4($t0)
    bnez  $t1, .L80246010
     addi  $t0, $t0, 8
    lui   $t2, %hi(nuBoot) # $t2, 0x8024
    lui   $sp, %hi(nuMainStack) # $sp, 0x8020
    addiu $t2, %lo(nuBoot) # addiu $t2, $t2, 0x6dc4
    jr    $t2
     addiu $sp, %lo(nuMainStack) # addiu $sp, $sp, 0xa00
    nop
    nop
    nop
    nop
    nop
    nop
