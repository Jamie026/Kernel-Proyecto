.section .data
msg: .string "Proceso 2 (RISC-V) dice: Revisando telemetría...\n"
len = . - msg

.section .text
.global _start

_start:
    // --- Syscall: write(fd, buffer, count) ---
    li a0, 1
    la a1, msg
    li a2, len
    li a7, 64
    ecall

    // --- Syscall: exit(code) ---
    li a0, 0
    li a7, 93
    ecall
