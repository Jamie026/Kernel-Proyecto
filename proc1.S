.section .data
msg: .ascii "Dato de telemetría: 1234\n"
len = . - msg

.section .text
.global _start

_start:
    # --- Syscall: write(fd, buffer, count) ---
    # Escribe en stdout (fd 1).
    # En escenarios 1-3: va a la consola.
    # En escenario 4: va a la pipe.
    li a0, 1        
    la a1, msg
    li a2, len
    li a7, 64       # syscall 64 (write)
    ecall

    # --- Syscall: exit(code) ---
    li a0, 0
    li a7, 93       # syscall 93 (exit)
    ecall