.equ SYS_WRITE, 64
.equ SYS_EXIT, 93

.section .data
msg_on:     .string "[P2] Escudo ACTIVADO\n"
len_on      = . - msg_on

msg_off:    .string "[P2] Escudo DESACTIVADO\n"
len_off     = . - msg_off

msg_stable: .string "[P2] Parametros estables. Mantiene estado.\n"
len_stable  = . - msg_stable

.section .text
.global _start

_start:
    lw t0, 8(sp)       # argv[1]
    lb t1, 0(t0)

    li t2, '1'
    beq t1, t2, print_on
    
    li t2, '2'
    beq t1, t2, print_stable

    # Si es '0' (o cualquier otra cosa)
    la a1, msg_off
    li a2, len_off
    j imprimir

print_on:
    la a1, msg_on
    li a2, len_on
    j imprimir

print_stable:
    la a1, msg_stable
    li a2, len_stable

imprimir:
    li a0, 1
    li a7, SYS_WRITE
    ecall

    li a0, 0
    li a7, SYS_EXIT
    ecall