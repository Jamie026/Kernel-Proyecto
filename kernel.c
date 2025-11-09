#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     // fork(), execvp(), sleep(), pipe(), dup2(), close(), kill()
#include <sys/wait.h>   // wait4()
#include <sys/time.h>   
#include <sys/resource.h> 
#include <signal.h>     // señales como SIGKILL
#include <string.h>     // Para strcmp, memset

// Escenario seleccionado por el usuario
static int escenario_actual = 0; 

// Almacenamiento global de estadísticas del ciclo
static struct rusage usage_p1, usage_p2, usage_p3;
static pid_t pid_p1 = 0, pid_p2 = 0, pid_p3 = 0;

// Resetea las estadísticas al inicio de cada ciclo
void inicializar_ciclo() {
    pid_p1 = pid_p2 = pid_p3 = 0;
    memset(&usage_p1, 0, sizeof(struct rusage));
    memset(&usage_p2, 0, sizeof(struct rusage));
    memset(&usage_p3, 0, sizeof(struct rusage));
}

// Reporta la terminación de un proceso
void proceso_terminado(const char *nombre_proceso, pid_t pid) {
    printf("\n[Kernel] Proceso %s (PID %d) terminado.\n", nombre_proceso, pid);
    fflush(stdout); // Asegura que el mensaje salga
}

// Imprime una fila de la tabla de recursos
void print_fila_tabla(const char *nombre, pid_t pid, struct rusage *usage) {
    printf("| %-10s | %-7d | %ld.%06ld s | %ld.%06ld s | %-12ld |\n",
           nombre,
           pid,
           usage->ru_utime.tv_sec, usage->ru_utime.tv_usec,
           usage->ru_stime.tv_sec, usage->ru_stime.tv_usec,
           usage->ru_maxrss);
}

// Imprime la tabla final de recursos del ciclo
void mostrar_tabla_recursos() {
    printf("\n--- Resumen de Recursos del Ciclo ---\n");
    printf("| Proceso    | PID     | CPU Usuario | CPU Sistema | Memoria (KB) |\n");
    printf("|------------|---------|-------------|-------------|--------------|\n");
    if (pid_p1 > 0) print_fila_tabla("./proceso1", pid_p1, &usage_p1);
    if (pid_p2 > 0) print_fila_tabla("./proceso2", pid_p2, &usage_p2);
    if (pid_p3 > 0) print_fila_tabla("./proceso3", pid_p3, &usage_p3);
    printf("|------------|---------|-------------|-------------|--------------|\n");
    fflush(stdout);
}

// Espera a un proceso (con o sin timeout) y guarda sus recursos
void esperar_proceso(pid_t pid, const char *nombre_proceso, int timeout_sec) {
    int status;
    struct rusage usage; // Temporal para recibir datos de wait4

    if (timeout_sec > 0) {
        // Espera con límite de tiempo
        printf("[Kernel] Esperando %d segundos (timeout activo para %s)...\n",
               timeout_sec, nombre_proceso);
        fflush(stdout);

        int remaining = timeout_sec;
        while (remaining > 0) {
            // WNOHANG no se bloquea, permite revisar en bucle
            pid_t terminated_pid = wait4(pid, &status, WNOHANG, &usage);
            if (terminated_pid == pid) {
                proceso_terminado(nombre_proceso, pid);
                goto guardar_stats; // Terminó a tiempo
            }
            sleep(1);
            remaining--;
        }

        // Timeout: Matar al proceso
        printf("[Kernel Error] %s (PID %d) no respondió. Terminando con SIGKILL...\n", 
               nombre_proceso, pid);
        kill(pid, SIGKILL);
        wait4(pid, &status, 0, &usage); // Recoger recursos
        proceso_terminado(nombre_proceso, pid);

    } else {
        // Espera normal (bloqueante)
        printf("[Kernel] Esperando a %s (PID %d)...\n", nombre_proceso, pid);
        fflush(stdout);
        wait4(pid, &status, 0, &usage);
        proceso_terminado(nombre_proceso, pid);
    }

guardar_stats:
    // Guarda las estadísticas en la variable global correcta
    if (strcmp(nombre_proceso, "./proceso1") == 0) {
        usage_p1 = usage;
        pid_p1 = pid;
    } else if (strcmp(nombre_proceso, "./proceso2") == 0) {
        usage_p2 = usage;
        pid_p2 = pid;
    } else if (strcmp(nombre_proceso, "./proceso3") == 0) {
        usage_p3 = usage;
        pid_p3 = pid;
    }
}

// Función del proceso hijo: solo hace execvp
void lanzar_hijo_exec(char *const argv[]) {
    execvp("qemu-riscv32", argv);
    perror("execvp"); // Se ejecuta solo si execvp falla
    exit(1);
}

// ESCENARIO 1: P1 -> P2 -> P3
void ejecutar_escenario_1() {
    pid_t pid1, pid2, pid3;
    const int TIMEOUT_P3 = 5;
    
    // --- 1. Lanzar todos los procesos ---
    if ((pid1 = fork()) == 0) {
        char *argv[] = {"qemu-riscv32", "./proceso1", NULL};
        lanzar_hijo_exec(argv);
    }
    printf("[Kernel] Lanzando ./proceso1 (PID %d)...\n", pid1);

    if ((pid2 = fork()) == 0) {
        char *argv[] = {"qemu-riscv32", "./proceso2", "0", NULL};
        lanzar_hijo_exec(argv);
    }
    printf("[Kernel] Lanzando ./proceso2 (PID %d)...\n", pid2);
    
    if ((pid3 = fork()) == 0) {
        char *argv[] = {"qemu-riscv32", "./proceso3", NULL};
        lanzar_hijo_exec(argv);
    }
    printf("[Kernel] Lanzando ./proceso3 (PID %d)...\n", pid3);

    // --- 2. Pausar los procesos que deben esperar ---
    printf("[Kernel] Pausando P2 (PID %d) y P3 (PID %d)\n", pid2, pid3);
    kill(pid2, SIGSTOP);
    kill(pid3, SIGSTOP);
    fflush(stdout); // Asegura que los mensajes salgan

    // --- 3. Ejecutar la cadena de dependencia ---
    esperar_proceso(pid1, "./proceso1", 0); // Espera a P1
    
    printf("[Kernel] Reanudando P2 (PID %d)\n", pid2);
    fflush(stdout);
    kill(pid2, SIGCONT); // Reanuda P2
    esperar_proceso(pid2, "./proceso2", 0); // Espera a P2

    printf("[Kernel] Reanudando P3 (PID %d)\n", pid3);
    fflush(stdout);
    kill(pid3, SIGCONT); // Reanuda P3
    esperar_proceso(pid3, "./proceso3", TIMEOUT_P3); // Espera a P3
}

// ESCENARIO 2: (P1 + P3) -> P2
void ejecutar_escenario_2() {
    pid_t pid1, pid2, pid3;
    int uart_pipe[2];
    if (pipe(uart_pipe) == -1) { perror("pipe"); exit(1); }

    // --- 1. Lanzar todos los procesos ---
    if ((pid3 = fork()) == 0) {
        // HIJO: P3 (Receptor)
        close(uart_pipe[1]);
        dup2(uart_pipe[0], STDIN_FILENO); // Redirige stdin
        close(uart_pipe[0]);
        char *argv[] = {"qemu-riscv32", "./proceso3", NULL};
        lanzar_hijo_exec(argv);
    }
    printf("[Kernel] Lanzando ./proceso3 (PID %d)...\n", pid3);

    if ((pid2 = fork()) == 0) {
        char *argv[] = {"qemu-riscv32", "./proceso2", "0", NULL};
        lanzar_hijo_exec(argv);
    }
    printf("[Kernel] Lanzando ./proceso2 (PID %d)...\n", pid2);

    if ((pid1 = fork()) == 0) {
        // HIJO: P1 (Emisor)
        close(uart_pipe[0]);
        dup2(uart_pipe[1], STDOUT_FILENO); // Redirige stdout
        close(uart_pipe[1]);
        char *argv[] = {"qemu-riscv32", "./proceso1", NULL};
        lanzar_hijo_exec(argv);
    }
    printf("[Kernel] Lanzando ./proceso1 (PID %d)...\n", pid1);

    // El padre cierra su copia del pipe
    close(uart_pipe[0]);
    close(uart_pipe[1]);
    
    // --- 2. Pausar los procesos que deben esperar ---
    printf("[Kernel] Pausando P3 (PID %d) y P2 (PID %d)\n", pid3, pid2);
    kill(pid3, SIGSTOP); // P3 espera a P1
    kill(pid2, SIGSTOP); // P2 espera a P1 y P3
    fflush(stdout);

    // --- 3. Ejecutar la cadena de dependencia ---
    esperar_proceso(pid1, "./proceso1", 0); // P1 se ejecuta
    
    printf("[Kernel] Reanudando P3 (PID %d)\n", pid3);
    fflush(stdout);
    kill(pid3, SIGCONT); // Reanuda P3
    esperar_proceso(pid3, "./proceso3", 0); // Espera a P3

    printf("[Kernel] Reanudando P2 (PID %d)\n", pid2);
    fflush(stdout);
    kill(pid2, SIGCONT); // Reanuda P2
    esperar_proceso(pid2, "./proceso2", 0); // Espera a P2
}

// ESCENARIO 3: P2 -> (P1 + P3)
void ejecutar_escenario_3() {
    pid_t pid1, pid2, pid3;
    int uart_pipe[2];
    if (pipe(uart_pipe) == -1) { perror("pipe"); exit(1); }

    // --- 1. Lanzar todos los procesos ---
    if ((pid2 = fork()) == 0) {
        char *argv[] = {"qemu-riscv32", "./proceso2", "1", NULL};
        lanzar_hijo_exec(argv);
    }
    printf("[Kernel] Lanzando ./proceso2 (PID %d)...\n", pid2);

    if ((pid3 = fork()) == 0) {
        close(uart_pipe[1]);
        dup2(uart_pipe[0], STDIN_FILENO);
        close(uart_pipe[0]);
        char *argv[] = {"qemu-riscv32", "./proceso3", NULL};
        lanzar_hijo_exec(argv);
    }
    printf("[Kernel] Lanzando ./proceso3 (PID %d)...\n", pid3);

    if ((pid1 = fork()) == 0) {
        close(uart_pipe[0]);
        dup2(uart_pipe[1], STDOUT_FILENO);
        close(uart_pipe[1]);
        char *argv[] = {"qemu-riscv32", "./proceso1", NULL};
        lanzar_hijo_exec(argv);
    }
    printf("[Kernel] Lanzando ./proceso1 (PID %d)...\n", pid1);

    close(uart_pipe[0]);
    close(uart_pipe[1]);
    
    // --- 2. Pausar los procesos que deben esperar ---
    printf("[Kernel] Pausando P1 (PID %d) y P3 (PID %d)\n", pid1, pid3);
    kill(pid1, SIGSTOP); // P1 espera a P2
    kill(pid3, SIGSTOP); // P3 espera a P1
    fflush(stdout);

    // --- 3. Ejecutar la cadena de dependencia ---
    esperar_proceso(pid2, "./proceso2", 0); // P2 se ejecuta
    
    printf("[Kernel] Reanudando P1 (PID %d)\n", pid1);
    fflush(stdout);
    kill(pid1, SIGCONT); // Reanuda P1
    esperar_proceso(pid1, "./proceso1", 0); // Espera a P1

    printf("[Kernel] Reanudando P3 (PID %d)\n", pid3);
    fflush(stdout);
    kill(pid3, SIGCONT); // Reanuda P3
    esperar_proceso(pid3, "./proceso3", 0); // Espera a P3
}

// Ejecuta el escenario principal
void ejecutar_escenario() {
    switch (escenario_actual) {
        case 1:
            printf("--- Escenario 1: [P1] -> [P2] -> [P3] (Concurrente-Pausado) ---\n");
            ejecutar_escenario_1();
            break;

        case 2:
            printf("--- Escenario 2: [(P1 + P3)] -> [P2] (Concurrente-Pausado) ---\n");
            ejecutar_escenario_2();
            break;

        case 3:
            printf("--- Escenario 3: [P2] -> [(P1 + P3)] (Concurrente-Pausado) ---\n");
            ejecutar_escenario_3();
            break;

        case 4:
            printf("--- Escenario 4: En desarrollo (modo automático con syscalls) ---\n");
            break;

        default:
            printf("[Kernel] Error: Escenario no válido.\n");
            break;
    }
}

// Punto de entrada principal
int main() {
    int ciclo = 1;
    printf("[Kernel] Iniciando el Orquestador de Satélite.\n");
    printf("El ciclo de ejecución se repetirá cada 10 segundos.\n");

    while (1) {
        printf("\n--- Inicio del ciclo #%d ---\n", ciclo);
        
        inicializar_ciclo(); // Resetea stats

        // Pide el escenario solo la primera vez
        if (escenario_actual == 0) {
            int temp;
            printf("Seleccione el escenario de ejecución:\n");
            printf("  1: P1 → P2 → P3\n");
            printf("  2: P1 → P3 → P2\n");
            printf("  3: P2 → P1 → P3\n");
            printf("  4: En desarrollo\n");
            printf("Escenario (1-4): ");

            if (scanf("%d", &temp) == 1 && temp >= 1 && temp <= 4)
                escenario_actual = temp;
            else {
                while (fgetc(stdin) != '\n'); // Limpia buffer
                printf("[Kernel] Error: opción inválida.\n");
            }
        }

        // Ejecuta el escenario seleccionado
        if (escenario_actual != 0) {
             printf("\n[Kernel] Ejecutando escenario persistente: %d...\n", escenario_actual);
             fflush(stdout);
            ejecutar_escenario();
        }

        mostrar_tabla_recursos(); // Imprime la tabla de resumen

        printf("--- Fin de ciclo #%d ---\n", ciclo);
        ciclo++;
        sleep(10); // Pausa antes de repetir
    }

    return 0;
}