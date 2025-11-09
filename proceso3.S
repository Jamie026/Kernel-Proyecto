.section .data
msg_prefijo: .string "Proceso 3 (RISC-V) recibió: "
len_prefijo = . - msg_prefijo

.section .bss
.align 2
buffer_rx: .space 128
TAM_BUFFER = 128

.section .text
.global _start

_start:
    # 1. Leer datos del pipe (stdin)
    li a0, 0        # fd 0 (stdin)
    la a1, buffer_rx
    li a2, TAM_BUFFER
    li a7, 63       # syscall read
    ecall
    mv s0, a0       # s0 = bytes leídos

    # 2. Imprimir prefijo
    li a0, 1
    la a1, msg_prefijo
    li a2, len_prefijo
    li a7, 64       # syscall write
    ecall

    # 3. Imprimir datos recibidos
    li a0, 1
    la a1, buffer_rx
    mv a2, s0       # Escribir los s0 bytes leídos
    li a7, 64
    ecall

    # 4. Salir
    li a0, 0
    li a7, 93       # syscall exit
    ecall