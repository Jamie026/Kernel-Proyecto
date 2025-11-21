# Constantes de Syscalls
.equ SYS_READ, 63
.equ SYS_WRITE, 64
.equ SYS_EXIT, 93

# Configuración del buffer
.equ BUF_SIZE, 1         # Leer byte por byte

.section .bss
.align 2
buffer: .space BUF_SIZE # Buffer de 1 byte

.section .text
.global _start

_start:
bucle_lectura:
    # 1. Leer un byte de stdin (fd 0)
    # En el escenario, esto viene del pipe de P1
    li a0, 0        # fd 0 (stdin)
    la a1, buffer
    li a2, 1        # Leer solo 1 byte
    li a7, SYS_READ # syscall read
    ecall
    mv s0, a0       # s0 = bytes leídos

    # Si no leímos nada (EOF por pipe cerrado) o error, salimos
    beqz s0, salir_exito # Si a0 == 0 (EOF), salir con éxito
    bltz s0, salir_error # Si a0 < 0 (error), salir con error

    # 2. Escribir el byte a stdout (fd 1)
    # El kernel redirigirá esto (en Escenario 1)
    li a0, 1        # fd 1 (stdout)
    la a1, buffer
    li a2, 1        # Escribir solo 1 byte
    li a7, SYS_WRITE # syscall write
    ecall
    bltz a0, salir_error # Salir si hay error de escritura

    # Detectar salto de línea, igual que P1
    lb t0, buffer        # Cargar el byte leído en t0
    li t1, 10            # Cargar ASCII de '\n' en t1
    bne t0, t1, bucle_lectura # Si no es salto de línea, seguir leyendo

    # Es un salto de línea, esperamos 1 segundo
    jal ra, esperar_1s
    j bucle_lectura      # Volver al bucle

# Funciones (Copiadas de P1)

#    Delay real de 1 segundo
esperar_1s:
    li t3, 0

delay_loop:
    # Este número es seguro (cabe en 32 bits)
    # Cada iteración tarda aprox ~40ns en QEMU.
    li t4, 50000000      # 50 millones ~55 ms

inner:
    addi t4, t4, -1
    bnez t4, inner
    addi t3, t3, 1
    li t5, 20            # repetir 20 veces
    blt t3, t5, delay_loop
    ret


salir_exito:
    li a0, 0
    li a7, SYS_EXIT
    ecall

salir_error:
    li a0, 1
    li a7, SYS_EXIT
    ecall