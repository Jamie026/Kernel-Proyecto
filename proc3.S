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
    # --- 1. Syscall: read(fd, buffer, count) ---
    # Lee de stdin (fd 0).
    # En escenarios 1-3: lee del teclado (y se bloquea).
    # En escenario 4: lee de la pipe.
    li a0, 0              # fd = 0 (stdin)
    la a1, buffer_rx
    li a2, TAM_BUFFER
    li a7, 63             # syscall 63 (read)
    ecall                 
    
    mv s0, a0 # Guardamos el número de bytes leídos

    # --- 2. Imprimir prefijo ---
    li a0, 1
    la a1, msg_prefijo
    li a2, len_prefijo
    li a7, 64
    ecall

    # --- 3. Imprimir datos recibidos ---
    li a0, 1
    la a1, buffer_rx
    mv a2, s0             # count = bytes leídos
    li a7, 64
    ecall

    # --- 4. Syscall: exit(code) ---
    li a0, 0
    li a7, 93
    ecall