# Constantes de Syscalls
.equ SYS_READ, 63
.equ SYS_WRITE, 64
.equ SYS_EXIT, 93

# Configuración del buffer
.equ BUF_SIZE, 1         # leer byte por byte, 8 bits es un char, no leemos un int como tal

.section .bss # .bss guarda variables sin valor inicial
.align 2 # Le dice al ensamblador que alinee lo que sigue a una dirección que sea múltiplo de 2^2 = 4 bytes
buffer: .space BUF_SIZE # Reservar espacio para el buffer de 1 byte

.section .text # Lo que le sigue es código ejecutable
.global _start # Punto de entrada del programa

_start:
bucle_lectura:
    # Leer un byte
    li a0, 0             # fd = 0 (stdin) Entrada fd = 1 (stdout) Salida fd = 2 (stderr)
    la a1, buffer        # buf = &buffer Donde guardara lo leido
    li a2, 1             # count = 1 Número de bytes a leer
    li a7, SYS_READ      # syscall number for read
    ecall
    mv s0, a0            # a0 = bytes leídos (resultado de read) lo movemos a s0 para volver a usar a0

    beqz s0, salir_exito # Si s0 == 0 (EOF), salir con éxito
    bltz s0, salir_error # Si s0 < 0 (error), salir con error

    # Escribir el byte
    li a0, 1            # fd = 1 (stdout)
    la a1, buffer       # buf = &buffer Contenido a escribir
    li a2, 1            # count = 1 Número de bytes a escribir
    li a7, SYS_WRITE    # syscall number for write
    ecall 
    bltz a0, salir_error # Salir si hay error de escritura

    # Detectar salto de línea
    lb t0, buffer        # Cargar el byte leído en t0 
    li t1, 10            # Cargar ASCII de salto de linea o "\n" en t1
    bne t0, t1, bucle_lectura # Si no es salto de línea, seguir leyendo. Ejemplo 105 -> 1-0-5 -> salto de línea

    # Espera antes de continuar
    jal ra, delay # Guarda en "ra" la dirección de retorno
    j bucle_lectura

# Delay por medio de bucles
delay:
    li t3, 0               # Inicializamos un contador externo en 0

delay_loop:
    li t4, 50000000        # Número grande, representa ~55ms por iteración

inner:
    addi t4, t4, -1        # Restamos 1 al contador interno
    bnez t4, inner         # Si aún no llega a 0, repetir

    addi t3, t3, 1         # Incrementamos el contador externo (pasó una iteración "inner")
    li t5, 10              # Queremos repetir 10 veces

    blt t3, t5, delay_loop # Si no hemos llegado a 10, repetir el "delay_loop"

    ret                    # Regresa al programa usando la dirección guardada en "ra"

salir_exito:
    li a0, 0 # Código de éxito 0
    li a7, SYS_EXIT
    ecall

salir_error:
    li a0, 1  # Código de error 1
    li a7, SYS_EXIT
    ecall