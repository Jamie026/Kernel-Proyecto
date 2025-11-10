.section .bss
.align 2
buffer_rx: .space 128
TAM_BUFFER = 128

.section .text
.global _start

_start:
    # 1. Leer datos de stdin (fd 0)
    # En el escenario, esto viene del pipe de P1
    li a0, 0        # fd 0 (stdin)
    la a1, buffer_rx
    li a2, TAM_BUFFER
    li a7, 63       # syscall read
    ecall
    mv s0, a0       # s0 = bytes leídos

    # Si no leímos nada (por error o EOF), salimos
    blez s0, salir

    # 2. Escribir datos a stdout (fd 1)
    # El kernel redirigirá esto a su propia pipe para leerlo
    li a0, 1        # fd 1 (stdout)
    la a1, buffer_rx
    mv a2, s0       # Escribir exactamente lo que leímos
    li a7, 64       # syscall write
    ecall

salir:
    # 3. Salir
    li a0, 0
    li a7, 93       # syscall exit
    ecall