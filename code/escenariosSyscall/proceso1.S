.equ SYS_READ, 63
.equ SYS_WRITE, 64
.equ SYS_EXIT, 93
.equ SYS_CLONE, 220
.equ SYS_EXECVE, 221
.equ SYS_WAIT4, 260

.section .data
prog_p3: .string "./code/escenariosSyscall/proceso3"

.align 4
argv_arr: .word 0, 0, 0

.section .bss
buffer:     .space 32
status_var: .word 0

.section .text
.global _start

_start:
    # s2 = Estado actual (1 = ON, 0 = OFF).
    # Por defecto arranca Desactivado (0)
    li s2, 0
    la s1, buffer

bucle_lectura:
    li a0, 0
    mv a1, s1
    li a2, 1
    li a7, SYS_READ
    ecall

    blez a0, check_eof

    lb t0, 0(s1)
    li t1, 10
    beq t0, t1, procesar_logica

    addi s1, s1, 1
    j bucle_lectura

check_eof:
    la t0, buffer
    beq s1, t0, salir_p1
    j procesar_logica

procesar_logica:
    sb zero, 0(s1)

    # ASCII A INT
    la t0, buffer
    li t1, 0
    li t2, 10

loop_atoi:
    lb t3, 0(t0)
    beqz t3, fin_atoi
    addi t3, t3, -48
    mul t1, t1, t2
    add t1, t1, t3
    addi t0, t0, 1
    j loop_atoi

fin_atoi:
    # t1 = valor leído
    
    # 1. Lógica de umbrales
    li t4, 90
    bgt t1, t4, quiere_activar

    li t4, 55
    blt t1, t4, quiere_desactivar

    # Rango medio: Mantener
    mv t5, s2 
    j comparar_estados

quiere_activar:
    li t5, 1
    j comparar_estados

quiere_desactivar:
    li t5, 0

comparar_estados:
    beq t5, s2, sin_cambios

    # HAY CAMBIO
    mv s2, t5           
    addi t6, s2, 48     # '0' o '1'
    j preparar_envio

sin_cambios:
    # NO HAY CAMBIO
    li t6, '2'  

preparar_envio:
    la s1, buffer
    sb t6, 0(s1)
    sb zero, 1(s1)

    # INVOCAR P3
    li a7, SYS_CLONE
    li a0, 17
    li a1, 0
    ecall
    
    beqz a0, soy_hijo_p1

    # PADRE P1
    
    # Pequeño delay ciego para sincronización (wait a P3)
    li t0, 4000000
blind_delay:
    addi t0, t0, -1
    bnez t0, blind_delay

    # Wait no bloqueante
    li a0, -1
    la a1, status_var
    li a2, 1
    li a3, 0
    li a7, SYS_WAIT4
    ecall

    # Antes de volver a leer el siguiente dato, esperamos.
    jal ra, delay

    # Reiniciar puntero de buffer y leer de nuevo
    la s1, buffer
    j bucle_lectura

soy_hijo_p1:
    la t0, prog_p3
    la t1, argv_arr
    la t2, buffer

    sw t0, 0(t1)
    sw t2, 4(t1)
    sw zero, 8(t1)

    mv a0, t0
    mv a1, t1
    li a2, 0
    li a7, SYS_EXECVE
    ecall
    
    li a0, 1
    li a7, SYS_EXIT
    ecall

salir_p1:
    li a0, 0
    li a7, SYS_EXIT
    ecall

delay:
    li t3, 0               # Inicializamos un contador externo en 0

delay_loop:
    li t4, 50000000        # Número grande (~50M ciclos)

inner:
    addi t4, t4, -1        # Restamos 1 al contador interno
    bnez t4, inner         # Si aún no llega a 0, repetir

    addi t3, t3, 1         # Incrementamos el contador externo
    li t5, 20              # Repetir 10 veces (Total ~500M ciclos)

    blt t3, t5, delay_loop # Si no hemos llegado a 10, repetir

    ret                    # Regresa a donde fue llamado (jal ra)