#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     // fork(), execvp(), sleep(), pipe(), dup2(), close(), kill(), read()
#include <sys/wait.h>   // wait4()
#include <sys/time.h>   
#include <sys/resource.h> 
#include <signal.h>     // señales como SIGKILL
#include <string.h>     // Para strcmp, memset

// Escenario seleccionado por el usuario
static int escenario_actual = 0; 

// Almacenamiento global de estadísticas
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
    fflush(stdout);
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
        usage_p1 = usage; pid_p1 = pid;
    } else if (strcmp(nombre_proceso, "./proceso2") == 0) {
        usage_p2 = usage; pid_p2 = pid;
    } else if (strcmp(nombre_proceso, "./proceso3") == 0) {
        usage_p3 = usage; pid_p3 = pid;
    }
}

// Función del hijo: solo ejecuta execvp
void lanzar_hijo_exec(char *const argv[]) {
    execvp("qemu-riscv32", argv);
    perror("execvp"); // Se ejecuta solo si execvp falla
    exit(1);
}

// Lee los datos que P3 envió al kernel por la pipe
void leer_datos_p3(int pipe_fd) {
    char buffer[128];
    // Lee del extremo de lectura de la pipe
    ssize_t bytes_read = read(pipe_fd, buffer, sizeof(buffer) - 1);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0'; // Asegura terminación de string
        // Limpia el \n final para un log más limpio
        if (buffer[bytes_read - 1] == '\n') buffer[bytes_read - 1] = '\0';
        
        printf("[Kernel] >>> DATOS RECIBIDOS de P3: [%s] <<<\n", buffer);
        fflush(stdout);
    } else {
        printf("[Kernel] No se recibieron datos de P3.\n");
    }
    close(pipe_fd); // Cierra el extremo de lectura
}

// ESCENARIO 1: P1 -> P2 -> P3
void ejecutar_escenario_1() {
    pid_t pid1, pid2, pid3;
    const int TIMEOUT_P3 = 5;
    int datos_pipe[2]; // Pipe para P3 -> Kernel
    if (pipe(datos_pipe) == -1) { perror("pipe datos"); exit(1); }

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
        // P3 escribe al kernel por datos_pipe[1]
        close(datos_pipe[0]); 
        dup2(datos_pipe[1], STDOUT_FILENO);
        close(datos_pipe[1]);
        // NOTA: P3 leerá del teclado (stdin) ya que no hay pipe P1->P3
        char *argv[] = {"qemu-riscv32", "./proceso3", NULL};
        lanzar_hijo_exec(argv);
    }
    printf("[Kernel] Lanzando ./proceso3 (PID %d)...\n", pid3);
    close(datos_pipe[1]); // El padre no escribe

    printf("[Kernel] Pausando P2 y P3\n");
    kill(pid2, SIGSTOP); kill(pid3, SIGSTOP); fflush(stdout);

    esperar_proceso(pid1, "./proceso1", 0);
    printf("[Kernel] Reanudando P2\n"); fflush(stdout);
    kill(pid2, SIGCONT); esperar_proceso(pid2, "./proceso2", 0);
    printf("[Kernel] Reanudando P3\n"); fflush(stdout);
    kill(pid3, SIGCONT); esperar_proceso(pid3, "./proceso3", TIMEOUT_P3);
    
    leer_datos_p3(datos_pipe[0]);
}

// ESCENARIO 2: (P1 + P3) -> P2
void ejecutar_escenario_2() {
    pid_t pid1, pid2, pid3;
    int uart_pipe[2], datos_pipe[2]; // Dos pipes
    if (pipe(uart_pipe) == -1) { perror("pipe uart"); exit(1); }
    if (pipe(datos_pipe) == -1) { perror("pipe datos"); exit(1); }

    if ((pid3 = fork()) == 0) {
        // P3 lee de uart_pipe[0], escribe a datos_pipe[1]
        close(uart_pipe[1]); dup2(uart_pipe[0], STDIN_FILENO); close(uart_pipe[0]);
        close(datos_pipe[0]); dup2(datos_pipe[1], STDOUT_FILENO); close(datos_pipe[1]);
        char *argv[] = {"qemu-riscv32", "./proceso3", NULL};
        lanzar_hijo_exec(argv);
    }
    printf("[Kernel] Lanzando ./proceso3 (PID %d)...\n", pid3);

    if ((pid2 = fork()) == 0) {
        // P2 no usa pipes, cierra todos los extremos
        close(uart_pipe[0]); close(uart_pipe[1]); close(datos_pipe[0]); close(datos_pipe[1]);
        char *argv[] = {"qemu-riscv32", "./proceso2", "0", NULL};
        lanzar_hijo_exec(argv);
    }
    printf("[Kernel] Lanzando ./proceso2 (PID %d)...\n", pid2);

    if ((pid1 = fork()) == 0) {
        // P1 escribe a uart_pipe[1]
        close(datos_pipe[0]); close(datos_pipe[1]); // No usa datos_pipe
        close(uart_pipe[0]); dup2(uart_pipe[1], STDOUT_FILENO); close(uart_pipe[1]);
        char *argv[] = {"qemu-riscv32", "./proceso1", NULL};
        lanzar_hijo_exec(argv);
    }
    printf("[Kernel] Lanzando ./proceso1 (PID %d)...\n", pid1);

    // Limpieza de pipes en el padre
    close(uart_pipe[0]); close(uart_pipe[1]); 
    close(datos_pipe[1]); // El padre solo lee de datos_pipe[0]
    
    printf("[Kernel] Pausando P3 y P2\n");
    kill(pid3, SIGSTOP); kill(pid2, SIGSTOP); fflush(stdout);

    esperar_proceso(pid1, "./proceso1", 0);
    printf("[Kernel] Reanudando P3\n"); fflush(stdout);
    kill(pid3, SIGCONT); esperar_proceso(pid3, "./proceso3", 0);
    
    // P3 ya terminó, podemos leer su respuesta
    leer_datos_p3(datos_pipe[0]);

    printf("[Kernel] Reanudando P2\n"); fflush(stdout);
    kill(pid2, SIGCONT); esperar_proceso(pid2, "./proceso2", 0);
}

// ESCENARIO 3: P2 -> (P1 + P3)
void ejecutar_escenario_3() {
    pid_t pid1, pid2, pid3;
    int uart_pipe[2], datos_pipe[2];
    if (pipe(uart_pipe) == -1) { perror("pipe uart"); exit(1); }
    if (pipe(datos_pipe) == -1) { perror("pipe datos"); exit(1); }

    if ((pid2 = fork()) == 0) {
        close(uart_pipe[0]); close(uart_pipe[1]); close(datos_pipe[0]); close(datos_pipe[1]);
        char *argv[] = {"qemu-riscv32", "./proceso2", "1", NULL};
        lanzar_hijo_exec(argv);
    }
    printf("[Kernel] Lanzando ./proceso2 (PID %d)...\n", pid2);

    if ((pid3 = fork()) == 0) {
        close(uart_pipe[1]); dup2(uart_pipe[0], STDIN_FILENO); close(uart_pipe[0]);
        close(datos_pipe[0]); dup2(datos_pipe[1], STDOUT_FILENO); close(datos_pipe[1]);
        char *argv[] = {"qemu-riscv32", "./proceso3", NULL};
        lanzar_hijo_exec(argv);
    }
    printf("[Kernel] Lanzando ./proceso3 (PID %d)...\n", pid3);

    if ((pid1 = fork()) == 0) {
        close(datos_pipe[0]); close(datos_pipe[1]);
        close(uart_pipe[0]); dup2(uart_pipe[1], STDOUT_FILENO); close(uart_pipe[1]);
        char *argv[] = {"qemu-riscv32", "./proceso1", NULL};
        lanzar_hijo_exec(argv);
    }
    printf("[Kernel] Lanzando ./proceso1 (PID %d)...\n", pid1);

    close(uart_pipe[0]); close(uart_pipe[1]); close(datos_pipe[1]);
    
    printf("[Kernel] Pausando P1 y P3\n");
    kill(pid1, SIGSTOP); kill(pid3, SIGSTOP); fflush(stdout);

    esperar_proceso(pid2, "./proceso2", 0);
    printf("[Kernel] Reanudando P1\n"); fflush(stdout);
    kill(pid1, SIGCONT); esperar_proceso(pid1, "./proceso1", 0);
    printf("[Kernel] Reanudando P3\n"); fflush(stdout);
    kill(pid3, SIGCONT); esperar_proceso(pid3, "./proceso3", 0);

    leer_datos_p3(datos_pipe[0]);
}

// Ejecuta el escenario principal
void ejecutar_escenario() {
    switch (escenario_actual) {
        case 1:
            printf("--- Escenario 1: [P1] -> [P2] -> [P3] ---\n");
            ejecutar_escenario_1();
            break;
        case 2:
            printf("--- Escenario 2: [(P1 + P3)] -> [P2] ---\n");
            ejecutar_escenario_2();
            break;
        case 3:
            printf("--- Escenario 3: [P2] -> [(P1 + P3)] ---\n");
            ejecutar_escenario_3();
            break;
        case 4:
            printf("--- Escenario 4: En desarrollo ---\n");
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
        inicializar_ciclo();

        // Pide el escenario solo la primera vez
        if (escenario_actual == 0) {
            int temp;
            printf("Seleccione el escenario de ejecución (1-4): ");
            if (scanf("%d", &temp) == 1 && temp >= 1 && temp <= 4) {
                escenario_actual = temp;
            } else {
                while (fgetc(stdin) != '\n'); // Limpia buffer
            }
        }

        // Ejecuta el escenario seleccionado
        if (escenario_actual != 0) {
             printf("\n[Kernel] Ejecutando escenario: %d...\n", escenario_actual);
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