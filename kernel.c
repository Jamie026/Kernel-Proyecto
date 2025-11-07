#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     // Para fork(), execvp(), sleep(), pipe(), dup2(), close(), alarm(), kill()
#include <sys/wait.h>   // Para wait4()
#include <sys/time.h>   
#include <sys/resource.h> 
#include <signal.h>     // Para la señal SIGKILL

// Variable global para mantener el escenario seleccionado
static int escenario_actual = 0; 

/**
 * @brief Reporta los recursos consumidos por un proceso terminado.
 */
void reportar_recursos(const char *nombre_proceso, pid_t pid, struct rusage *usage) {
    printf("\n[Kernel C] Proceso %s (PID %d) terminado.", nombre_proceso, pid);
    printf("\n\t--- Uso de Recursos ---\n");
    long user_sec = usage->ru_utime.tv_sec;
    long user_usec = usage->ru_utime.tv_usec;
    printf("\t- Tiempo CPU (Usuario): %ld.%06ld segundos\n", user_sec, user_usec);
    long sys_sec = usage->ru_stime.tv_sec;
    long sys_usec = usage->ru_stime.tv_usec;
    printf("\t- Tiempo CPU (Sistema): %ld.%06ld segundos\n", sys_sec, sys_usec);
    printf("\t- Memoria Máx. (RSS): %ld KB\n", usage->ru_maxrss);
    printf("\t------------------------\n");
}

/**
 * @brief Lanza un proceso RISC-V único y espera a que termine.
 * @param timeout_sec Si es > 0, espera ese tiempo antes de matar al proceso.
 */
void lanzar_proceso_secuencial(const char *nombre_proceso, int timeout_sec) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork"); exit(1);
    } else if (pid == 0) {
        // HIJO
        printf("\n[Kernel C] Lanzando %s (PID %d)...", nombre_proceso, getpid());
        fflush(stdout); 
        char *argv[] = {"qemu-riscv32", (char *)nombre_proceso, NULL};
        execvp("qemu-riscv32", argv);
        perror("execvp"); exit(1);
    } else {
        // PADRE
        int status;
        struct rusage usage;
        pid_t terminated_pid;
        
        // Si hay timeout (solo para proc3 en escenarios 1, 2, 3)
        if (timeout_sec > 0) {
            printf("[Kernel C] Esperando %d segundos. (Si proc3 se bloquea, será terminado).\n", timeout_sec);
            fflush(stdout);
            
            // Espera con límite de tiempo. waitpid no soporta fácilmente timeout, 
            // así que usamos sleep() en combinación con waitpid/wait4 con WNOHANG
            int remaining_time = timeout_sec;
            while (remaining_time > 0) {
                terminated_pid = wait4(pid, &status, WNOHANG, &usage);
                if (terminated_pid == pid) {
                    // El proceso terminó (éxito o error)
                    reportar_recursos(nombre_proceso, pid, &usage);
                    return;
                }
                sleep(1);
                remaining_time--;
            }

            // Si el bucle termina, el proceso sigue vivo
            if (kill(pid, SIGKILL) == 0) {
                printf("❌ [Kernel C ERROR] Proceso %s (PID %d) Terminado por Timeout (Señal SIGKILL).\n", nombre_proceso, pid);
                printf("Esto indica que el proceso se bloqueó esperando datos.\n");
                
                // Limpiar el zombie que acabamos de matar
                wait4(pid, &status, 0, &usage); 
                // Reportamos con ru_maxrss > 0 pero tiempos en 0, pues fue abortado.
                reportar_recursos(nombre_proceso, pid, &usage); 
                return;
            } else {
                perror("kill");
            }
        
        } else {
            // Comportamiento normal (espera indefinida)
            if (wait4(pid, &status, 0, &usage) == -1) {
                perror("wait4"); return;
            }
            reportar_recursos(nombre_proceso, pid, &usage);
        }
    }
}

/** 
 * @brief Lanza proc1 y proc3 concurrentemente con una pipe (UART simulada).
 */
void lanzar_uart_concurrentes() {
    int uart_pipe[2];
    pid_t pid1, pid3;
    struct rusage usage1, usage3;
    int status1, status3;

    if (pipe(uart_pipe) == -1) { perror("pipe"); exit(1); }

    // --- Proceso 1 (Emisor) ---
    pid1 = fork();
    if (pid1 == -1) { perror("fork proc1"); exit(1); }
    if (pid1 == 0) {
        close(uart_pipe[0]); dup2(uart_pipe[1], 1); close(uart_pipe[1]);
        char *argv[] = {"qemu-riscv32", "./proc1", NULL};
        execvp("qemu-riscv32", argv);
        perror("execvp proc1"); exit(1);
    }

    // --- Proceso 3 (Receptor) ---
    pid3 = fork();
    if (pid3 == -1) { perror("fork proc3"); exit(1); }
    if (pid3 == 0) {
        close(uart_pipe[1]); dup2(uart_pipe[0], 0); close(uart_pipe[0]);
        
        printf("\n[Kernel C] Lanzando %s (PID %d)...", "./proc3", getpid());
        fflush(stdout); 
        char *argv[] = {"qemu-riscv32", "./proc3", NULL};
        execvp("qemu-riscv32", argv);
        perror("execvp proc3"); exit(1);
    }

    // --- PADRE (Kernel) ---
    close(uart_pipe[0]);
    close(uart_pipe[1]);

    printf("\n[Kernel C] Esperando a proc1 (Emisor) y proc3 (Receptor)...");
    fflush(stdout);
    
    if (wait4(pid1, &status1, 0, &usage1) == -1) perror("wait4 pid1");
    reportar_recursos("./proc1", pid1, &usage1);
    
    if (wait4(pid3, &status3, 0, &usage3) == -1) perror("wait4 pid3");
    reportar_recursos("./proc3", pid3, &usage3);
}

// ##################################################################
// #####               SCHEDULER (main)                         #####
// ##################################################################

void ejecutar_escenario() {
    // Definimos el timeout para proc3 en modo secuencial
    const int TIMEOUT_PROC3 = 5; 

    switch (escenario_actual) {
        case 1:
            printf("--- Escenario 1: [P1] -> [P2] -> [P3] ---\n");
            lanzar_proceso_secuencial("./proc1", 0); // 0 = sin timeout
            lanzar_proceso_secuencial("./proc2", 0);
            printf("\n[Kernel C] Análisis de Error: P3 intentará leer de STDIN y fallará.\n");
            lanzar_proceso_secuencial("./proc3", TIMEOUT_PROC3); // P3 con timeout
            break;
        
        case 2:
            printf("--- Escenario 2: [P1] -> [P3] -> [P2] ---\n");
            lanzar_proceso_secuencial("./proc1", 0);
            printf("\n[Kernel C] Análisis de Error: P3 intentará leer de STDIN y fallará.\n");
            lanzar_proceso_secuencial("./proc3", TIMEOUT_PROC3); // P3 con timeout
            lanzar_proceso_secuencial("./proc2", 0);
            break;

        case 3:
            printf("--- Escenario 3: [P2] -> [P1] -> [P3] ---\n");
            lanzar_proceso_secuencial("./proc2", 0);
            lanzar_proceso_secuencial("./proc1", 0);
            printf("\n[Kernel C] Análisis de Error: P3 intentará leer de STDIN y fallará.\n");
            lanzar_proceso_secuencial("./proc3", TIMEOUT_PROC3); // P3 con timeout
            break;

        case 4:
            printf("--- Escenario 4: [(P1 || P3)] -> [P2] (Comunicación OK) ---\n");
            lanzar_uart_concurrentes(); // Lanza P1 y P3 juntos
            lanzar_proceso_secuencial("./proc2", 0); // Lanza P2 después
            break;

        default:
            printf("[Kernel C] Error: Escenario no válido. Elija uno.\n");
            break;
    }
}


int main() {
    int ciclo = 1;
    printf("[Kernel C] Iniciando el Orquestador de Satélite.\n");
    printf("El ciclo de ejecución se repetirá cada 10 segundos.\n");

    while (1) {
        printf("\n======================================================\n");
        printf("            INICIO DE CICLO #%d\n", ciclo);
        printf("======================================================\n");

        if (escenario_actual == 0) {
            // Solo pide escenario en el primer ciclo o si hay un error
            int temp_escenario = 0;
            printf("Seleccione el escenario de ejecución:\n");
            printf("  1: P1 -> P2 -> P3 (Secuencial, UART fallará)\n");
            printf("  2: P1 -> P3 -> P2 (Secuencial, UART fallará)\n");
            printf("  3: P2 -> P1 -> P3 (Secuencial, UART fallará)\n");
            printf("  4: (P1 || P3) -> P2 (Concurrente, UART OK)\n");
            printf("Escenario (1-4): ");
            
            if (scanf("%d", &temp_escenario) == 1 && temp_escenario >= 1 && temp_escenario <= 4) {
                escenario_actual = temp_escenario;
            } else {
                while (fgetc(stdin) != '\n'); // Limpiar buffer
                printf("[Kernel C] Error: Opción inválida.\n");
            }
        }
        
        printf("\n[Kernel C] Ejecutando escenario persistente: %d...\n", escenario_actual);

        ejecutar_escenario();

        printf("\n------------------------------------------------------\n");
        printf("FIN DE CICLO #%d.\n", ciclo);
        printf("------------------------------------------------------\n");

        ciclo++;
        sleep(10); // Pausa de 10 segundos
    }

    return 0;
}