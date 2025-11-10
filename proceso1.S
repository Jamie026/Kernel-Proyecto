# Constantes de Syscalls
.equ SYS_OPEN, 56
.equ SYS_READ, 63
.equ SYS_WRITE, 64
.equ SYS_CLOSE, 57
.equ SYS_EXIT, 93

# Constantes de Banderas (Flags)
.equ O_RDONLY, 0
.equ O_WRONLY, 1
.equ O_CREAT, 64
.equ O_TRUNC, 512

# Constantes del Programa
.equ BUF_SIZE, 64       # Tamaño máx. para leer y descartar
.equ INT_SIZE, 4        # 4 bytes para un entero de 32-bit
.equ NEWLINE_CHAR, 10   # ASCII para '\n'

.section .data
filename:       .asciz "medidas.txt"
offset_filename: .asciz "p1_offset.bin" # Archivo para guardar el estado
msg_open_err:   .asciz "Error P1: No se pudo abrir medidas.txt\n"
msg_offset_err: .asciz "Error P1: No se pudo escribir el offset\n"
msg_discard_err: .asciz "Error P1: Fallo al descartar bytes\n"

.section .bss
.align 2
current_offset: .space INT_SIZE # Variable para guardar el offset (4 bytes)
buffer:         .space BUF_SIZE # Buffer temporal

.section .text
.global _start

_start:
    # 1. Leer el offset guardado de p1_offset.bin
    li a0, -100
    la a1, offset_filename
    li a2, O_RDONLY
    li a7, SYS_OPEN
    ecall
    mv s0, a0
    bltz s0, inicializar_offset # Si no existe, saltar a inicializar a 0

    # Si existe, leer el valor (4 bytes)
    mv a0, s0
    la a1, current_offset
    li a2, INT_SIZE
    li a7, SYS_READ
    ecall
    mv a0, s0
    li a7, SYS_CLOSE
    ecall
    j abrir_archivo_datos

inicializar_offset:
    # Si p1_offset.bin no existía, el offset es 0
    li a0, 0
    la t0, current_offset
    sw a0, 0(t0)                # sw (Store Word) guarda el 0 en la variable

abrir_archivo_datos:
    # 2. Abrir el archivo de datos (medidas.txt)
    li a0, -100
    la a1, filename
    li a2, O_RDONLY
    li a7, SYS_OPEN
    ecall
    mv s1, a0                   # s1 = fd de medidas.txt
    bltz s1, salir_error_abrir

    # 3. Avanzar el cursor (reemplazo de lseek)
    la t0, current_offset
    lw s2, 0(t0)                # s2 = offset_a_buscar
    beq s2, zero, leer_linea_datos # Si offset es 0, saltar a leer

bucle_descartar:
    # Bucle para "buscar" (avanzar) el cursor del archivo
    # leyendo y descartando datos hasta llegar al offset guardado.
    
    # Decidir cuántos bytes leer: BUF_SIZE o lo que falte (s2)
    li t0, BUF_SIZE
    bge t0, s2, descartar_restante
    mv t1, t0                   # Leer un chunk de BUF_SIZE
    j leer_descarte
descartar_restante:
    mv t1, s2                   # Leer solo los bytes restantes (s2)

leer_descarte:
    mv a0, s1                   # fd
    la a1, buffer               # Usar buffer temporal para descartar
    mv a2, t1                   # bytes_a_leer
    li a7, SYS_READ
    ecall
    # a0 = bytes realmente leídos

    bltz a0, salir_error_descartar # Error
    beq a0, zero, leer_linea_datos # EOF inesperado, saltar a leer (no leerá nada)

    sub s2, s2, a0              # Restar bytes leídos al offset pendiente
    bgtz s2, bucle_descartar    # Si falta, seguir descartando

leer_linea_datos:
    # 4. Bucle: Leer la línea byte a byte
    # El cursor del archivo ya está en la posición correcta.
    mv s4, zero                 # s4 = índice del buffer
    mv s5, zero                 # s5 = contador de bytes de la línea actual

bucle_leer_byte:
    mv a0, s1                   # fd
    la a1, buffer
    add a1, a1, s4              # Apuntar a &buffer[s4]
    li a2, 1                    # Leer 1 byte
    li a7, SYS_READ
    ecall
    mv s3, a0                   # s3 = resultado de read (1 o 0)

    beq s3, zero, fin_bucle     # Si s3=0, es EOF -> salir del bucle

    addi s5, s5, 1              # Contar 1 byte leído
    la t0, buffer
    add t0, t0, s4
    lb t1, 0(t0)                # lb (Load Byte) carga el byte leído
    addi s4, s4, 1              # Incrementar índice del buffer

    li t2, NEWLINE_CHAR
    beq t1, t2, fin_bucle       # Si es '\n', fin de línea

    li t2, BUF_SIZE
    bge s4, t2, fin_bucle       # Si buffer lleno, salir
    
    j bucle_leer_byte

fin_bucle:
    # 5. Escribir la línea leída en stdout (pipe)
    beq s4, zero, comprobar_eof # Si no leímos nada (s4=0), no escribir
    
    li a0, 1                    # fd 1 (stdout)
    la a1, buffer
    mv a2, s4                   # Escribir s4 bytes (la línea)
    li a7, SYS_WRITE
    ecall

comprobar_eof:
    # 6. Calcular y guardar el próximo offset
    bnez s3, no_eof             # Si s3 != 0 (no fue EOF), calcular offset
    
    # Caso 1: Fue EOF (s3=0). Resetea el offset a 0 para el próximo ciclo.
    mv s2, zero
    j guardar_offset

no_eof:
    # Caso 2: No fue EOF. Calcular nuevo offset.
    la t0, current_offset
    lw s2, 0(t0)                # Cargar offset viejo
    add s2, s2, s5              # nuevo_offset = viejo + bytes_leidos_esta_vez

guardar_offset:
    # Guardar s2 (el nuevo offset) en la variable
    la t0, current_offset
    sw s2, 0(t0)

    # 7. Escribir el nuevo offset en p1_offset.bin
    li a0, -100
    la a1, offset_filename
    li a2, O_WRONLY | O_CREAT | O_TRUNC # Sobrescribir el archivo de estado
    li a3, 0644
    li a7, SYS_OPEN
    ecall
    mv s0, a0
    bltz s0, salir_error_offset

    mv a0, s0
    la a1, current_offset
    li a2, INT_SIZE             # Escribir 4 bytes
    li a7, SYS_WRITE
    ecall
    mv a0, s0
    li a7, SYS_CLOSE
    ecall

cerrar_archivo_datos:
    # 8. Cerrar archivo de datos
    mv a0, s1
    li a7, SYS_CLOSE
    ecall

salir_exito:
    li a0, 0
    li a7, SYS_EXIT
    ecall

salir_error_abrir:
    li a0, 2
    la a1, msg_open_err
    li a2, 38
    li a7, SYS_WRITE
    ecall
    li a0, 1
    li a7, SYS_EXIT
    ecall

salir_error_offset:
    li a0, 2
    la a1, msg_offset_err
    li a2, 42
    li a7, SYS_WRITE
    ecall
    li a0, 1
    li a7, SYS_EXIT
    ecall

salir_error_descartar:
    li a0, 2
    la a1, msg_discard_err
    li a2, 32
    li a7, SYS_WRITE
    ecall
    li a0, 1
    li a7, SYS_EXIT
    ecall