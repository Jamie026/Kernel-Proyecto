## Compilación

Para generar los archivos binarios se deben ejecutar los siguientes comandos:

```bash
riscv64-linux-gnu-gcc -mabi=ilp32 -march=rv32g -nostdlib -static -o proceso1 proceso1.S
riscv64-linux-gnu-gcc -mabi=ilp32 -march=rv32g -nostdlib -static -o proceso2 proceso2.S
riscv64-linux-gnu-gcc -mabi=ilp32 -march=rv32g -nostdlib -static -o proceso3 proceso3.S
gcc -o kernel kernel.c
```

Luego debe colocar el siguiente comando para ejecutar el kernel

```bash
./kernel
```
