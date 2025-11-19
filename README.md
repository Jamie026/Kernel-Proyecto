## Compilación

Para generar los archivos binarios para los primeros 3 escenarios, se deben ejecutar los siguientes comandos:

```bash
riscv64-linux-gnu-gcc -mabi=ilp32 -march=rv32g -nostdlib -static -o ./code/escenariosBasicos/proceso1 ./code/escenariosBasicos/proceso1.S
riscv64-linux-gnu-gcc -mabi=ilp32 -march=rv32g -nostdlib -static -o ./code/escenariosBasicos/proceso2 ./code/escenariosBasicos/proceso2.S
riscv64-linux-gnu-gcc -mabi=ilp32 -march=rv32g -nostdlib -static -o ./code/escenariosBasicos/proceso3 ./code/escenariosBasicos/proceso3.S
gcc -o kernel kernel.c
```

Luego se debe colocar el siguiente comando para ejecutar el kernel:

```bash
./kernel
```

Link del documento con explicación del codigo:

```bash
https://docs.google.com/document/d/1GNW5cXfPfhlicpvOLGHvWKd9brQbPIR_reR7-qulXe0/edit?usp=sharing
```

## Explicación Métricas:

# Ejemplo Escenario 2:

--- Resumen de Recursos del Ciclo ---

| Proceso | PID | T. Real (s) | CPU Usuario | CPU Sistema | Memoria Pico (KB) |

|----------------------|---------|-------------|-------------|-------------|-------------------|

| Receptor de Señal | 10308 | 60.000573 | 57.163918 s | 0.005999 s | 7760 |

| Control Escudo | 10969 | 0.003062 | 0.000000 s | 0.003050 s | 7720 |

| Analizador Espectral | 10309 | 60.001359 | 55.704248 s | 0.024986 s | 7712 |

|----------------------|---------|-------------|-------------|-------------|-------------------|

--- Métricas Avanzadas de Ejecución (E2/E3) ---

Proceso: Receptor de Señal (PID 10308)

[METRICAS AVANZADAS]

-   T. Ejecución Efectiva (CPU): 57.169917 s

-   Cambios de Contexto (Vol/Inv): 26 / 961

-   Pausas Totales: 6

-   T. Pausado Total (Wall): 25.017017 s

-   Quantum Dado/Usado: 60.00 s / 60.00 s

-   Estado Final: TERMINADO_EXIT (Status: 0)

Proceso: Control Escudo (PID 10969)

[METRICAS AVANZADAS]

-   T. Ejecución Efectiva (CPU): 0.003050 s

-   Cambios de Contexto (Vol/Inv): 2 / 0

-   Estado Final: TERMINADO_EXIT (Status: 0)

Proceso: Analizador Espectral (PID 10309)

[METRICAS AVANZADAS]

-   T. Ejecución Efectiva (CPU): 55.729234 s

-   Cambios de Contexto (Vol/Inv): 44 / 3238

-   Pausas Totales: 12

-   T. Pausado Total (Wall): 60.034829 s

-   Quantum Dado/Usado: 60.00 s / 60.00 s

-   Estado Final: TERMINADO_EXIT (Status: 0)

---

Tiempo total del ciclo: 120.039693 segundos
