.section .data
msg_err: .string "Error: Se requiere un parametro (0 o 1)\n"
len_err = . - msg_err
msg_on:  .string "Escudo de calor ACTIVADO\n"
len_on  = . - msg_on
msg_off: .string "Escudo de calor DESACTIVADO\n"
len_off = . - msg_off

.section .text
.global _start

_start:
    // En RV32, los punteros son de 4 bytes.
    // 0(sp) -> argc
    // 4(sp) -> argv[0] (nombre del programa)
    // 8(sp) -> argv[1] (primer argumento)

    lw t0, 0(sp)          // Cargar argc (32 bits)
    li t1, 2
    blt t0, t1, error     // Si argc < 2, error

    lw t1, 8(sp)          // Cargar puntero a argv[1] (offset 8 en RV32)
    lb t2, 0(t1)          // Cargar el primer carácter del argumento

    li t3, '0'
    beq t2, t3, desactivar
    li t3, '1'
    beq t2, t3, activar

    j error

activar:
    la a1, msg_on
    li a2, len_on
    j imprimir

desactivar:
    la a1, msg_off
    li a2, len_off
    j imprimir

error:
    la a1, msg_err
    li a2, len_err

imprimir:
    li a0, 1              // stdout
    li a7, 64             // syscall write
    ecall

salir:
    li a0, 0
    li a7, 93             // syscall exit
    ecall