.equ SYS_EXIT, 93
.equ SYS_CLONE, 220
.equ SYS_EXECVE, 221
.equ SYS_WAIT4, 260

.section .data
prog_p2: .string "./code/escenariosSyscall/proceso2"
arg_0:   .string "0"    # Desactivar
arg_1:   .string "1"    # Activar
arg_2:   .string "2"    # Mantener 

.align 4
argv_arr: .word 0, 0, 0

.section .bss
status_var_p3: .word 0

.section .text
.global _start

_start:
    lw t0, 0(sp)
    li t1, 2
    blt t0, t1, salir_error

    lw s1, 8(sp)        # argv[1] desde P1

    lb t2, 0(s1)
    
    li t3, '1'
    beq t2, t3, set_activar
    
    li t3, '2'
    beq t2, t3, set_mantener

    # Por descarte es '0'
    la s2, arg_0
    j lanzar_p2

set_activar:
    la s2, arg_1
    j lanzar_p2

set_mantener:
    la s2, arg_2

lanzar_p2:
    mv s3, s2

    # --- FORK P2 ---
    li a7, SYS_CLONE
    li a0, 17
    li a1, 0
    ecall

    beqz a0, soy_hijo_p3

    # --- PADRE P3 ---
    li t0, 2000000
blind_delay_p3:
    addi t0, t0, -1
    bnez t0, blind_delay_p3

    li a0, -1
    la a1, status_var_p3
    li a2, 1
    li a3, 0
    li a7, SYS_WAIT4
    ecall

    li a0, 0
    li a7, SYS_EXIT
    ecall

soy_hijo_p3:
    la t0, prog_p2
    la t1, argv_arr
    
    sw t0, 0(t1)
    sw s3, 4(t1)
    sw zero, 8(t1)

    mv a0, t0
    mv a1, t1
    li a2, 0
    li a7, SYS_EXECVE
    ecall

    li a0, 1
    li a7, SYS_EXIT
    ecall

salir_error:
    li a0, 1
    li a7, SYS_EXIT
    ecall