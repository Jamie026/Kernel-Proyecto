#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // Contiene fork, pipe, execvp, sleep, dup2, close
#include <sys/wait.h> // Contiene wait4
#include <sys/time.h> // Necesario para sys/resource.h
// Contiene 'rusage' para obtener estadísticas (uso de CPU, memoria).
#include <sys/resource.h> 
// Para manejar señales (como SIGKILL, SIGSTOP, SIGCONT, SIGTSTP).
#include <signal.h> 
#include <string.h> // Contiene memset y strstr

// --- Definiciones de colores ANSI (para formato de salida) ---
#define ANSI_RESET      "\x1B[0m"
#define ANSI_RED        "\x1B[31m"
#define ANSI_GREEN      "\x1B[32m"
#define ANSI_YELLOW     "\x1B[33m"
#define ANSI_BLUE       "\x1B[34m"
#define ANSI_MAGENTA    "\x1B[35m"
#define ANSI_CYAN       "\x1B[36m"
#define ANSI_WHITE      "\x1B[37m"
#define ANSI_BR_RED     "\x1B[91m"
#define ANSI_BR_GREEN   "\x1B[92m"
#define ANSI_BR_YELLOW  "\x1B[93m"
#define ANSI_BR_CYAN    "\x1B[96m"

#define COLOR_KERNEL    ANSI_CYAN
#define COLOR_P1        ANSI_BR_GREEN
#define COLOR_P2        ANSI_YELLOW
#define COLOR_P3        ANSI_BLUE
#define COLOR_ERROR     ANSI_BR_RED
#define COLOR_TABLE     ANSI_WHITE
#define COLOR_CICLO     ANSI_BR_YELLOW

// --- Variables Globales ---
static int escenario_actual = 0;
static int ciclo_actual = 1;
// 'rusage' es una estructura para guardar estadísticas de uso de recursos (CPU, memoria).
static struct rusage usage_p1, usage_p2, usage_p3;
// 'pid_t' es un tipo de dato estándar para almacenar IDs de Proceso (PID).
static pid_t pid_p1 = 0, pid_p2 = 0, pid_p3 = 0;

void inicializar_ciclo() {
    pid_p1 = pid_p2 = pid_p3 = 0;
    // 'memset' rellena un bloque de memoria con un valor específico.
    // Aquí, se usa para rellenar con ceros (0) las estructuras 'usage',
    // limpiando las estadísticas del ciclo anterior.
    memset(&usage_p1, 0, sizeof(struct rusage));
    memset(&usage_p2, 0, sizeof(struct rusage));
    memset(&usage_p3, 0, sizeof(struct rusage));
}

// --- Funciones auxiliares para imprimir en pantalla ---

const char* nombre_legible(const char *nombre) {
    if (strstr(nombre, "proceso1")) return "Proceso #1";
    if (strstr(nombre, "proceso2")) return "Proceso #2";
    if (strstr(nombre, "proceso3")) return "Proceso #3";
    return nombre;
}

const char* color_proceso(const char *nombre) {
    if (strstr(nombre, "proceso1")) return COLOR_P1;
    if (strstr(nombre, "proceso2")) return COLOR_P2;
    if (strstr(nombre, "proceso3")) return COLOR_P3;
    return ANSI_WHITE;
}

void proceso_terminado(const char *nombre_proceso, pid_t pid) {
    printf("\n" COLOR_KERNEL "[Kernel] " ANSI_RESET "%s%s (PID %d) terminado." ANSI_RESET "\n",
           color_proceso(nombre_proceso), nombre_legible(nombre_proceso), pid);
    // 'fflush' fuerza a que la salida (stdout) se imprima en la terminal
    // inmediatamente, sin esperar a que el buffer se llene.
    fflush(stdout);
}

// Imprime los datos clave de 'rusage':
// ru_utime: Tiempo de CPU en modo usuario (lo que gastó el proceso).
// ru_stime: Tiempo de CPU en modo sistema (lo que gastó el kernel para el proceso).
// ru_maxrss: Memoria RAM máxima usada (en KB).
void print_fila_tabla(const char *nombre, pid_t pid, struct rusage *usage) {
    printf("%s| %-10s | %-7d | %ld.%06ld s | %ld.%06ld s | %-12ld |" ANSI_RESET "\n",
           color_proceso(nombre),
           nombre_legible(nombre), pid,
           usage->ru_utime.tv_sec, usage->ru_utime.tv_usec, // Tiempo Usuario
           usage->ru_stime.tv_sec, usage->ru_stime.tv_usec, // Tiempo Sistema
           usage->ru_maxrss); // Memoria Max
}

void mostrar_tabla_recursos() {
    printf(COLOR_TABLE "\n--- Resumen de Recursos del Ciclo ---\n");
    printf("| Proceso    | PID     | CPU Usuario | CPU Sistema | Memoria (KB) |\n");
    printf("|------------|---------|-------------|-------------|--------------|\n" ANSI_RESET);
    if (pid_p1 > 0) print_fila_tabla("./proceso1", pid_p1, &usage_p1);
    if (pid_p2 > 0) print_fila_tabla("./proceso2", pid_p2, &usage_p2);
    if (pid_p3 > 0) print_fila_tabla("./proceso3", pid_p3, &usage_p3);
    printf(COLOR_TABLE "|------------|---------|-------------|-------------|--------------|\n" ANSI_RESET);
    fflush(stdout);
}

// --- Lógica principal de manejo de procesos ---

void esperar_proceso(pid_t pid, const char *nombre_proceso, int timeout_sec) {
    int status;
    struct rusage usage; // Estructura local para recibir las estadísticas

    if (timeout_sec > 0) {
        printf(COLOR_KERNEL "[Kernel] Esperando %d s (timeout activo para " ANSI_RESET "%s%s" COLOR_KERNEL ")..." ANSI_RESET "\n",
               timeout_sec, color_proceso(nombre_proceso), nombre_legible(nombre_proceso));
        fflush(stdout);

        for (int remaining = timeout_sec; remaining > 0; remaining--) {
            // 'wait4' es como waitpid, pero además puede obtener estadísticas de 'rusage' del hijo.
            // 'WNOHANG' (No Colgar) es clave: hace que wait4 revise si el 'pid' terminó,
            // pero *no se bloquea* si sigue corriendo (retorna 0 en ese caso).
            pid_t terminado = wait4(pid, &status, WNOHANG, &usage);
            
            if (terminado == pid) {
                // El proceso terminó antes del timeout
                proceso_terminado(nombre_proceso, pid);
                goto guardar_stats; // Salta a la etiqueta 'guardar_stats' al final
            }
            sleep(1); // Espera 1 segundo y vuelve a chequear
        }

        // Si el bucle termina, el proceso no respondió (timeout).
        printf(COLOR_ERROR "[Kernel Error] " ANSI_RESET "%s%s (PID %d) no respondió. Terminando con SIGKILL..." ANSI_RESET "\n",
               color_proceso(nombre_proceso), nombre_legible(nombre_proceso), pid);
        
        // 'kill' envía una señal a un proceso.
        // 'SIGKILL' (Señal 9) es una terminación forzada que el proceso no puede ignorar.
        kill(pid, SIGKILL);
        
        // Ahora sí esperamos (con 0, o sea, *bloqueando*) a que el proceso
        // termine (ahora que le enviamos SIGKILL) y recogemos sus estadísticas.
        wait4(pid, &status, 0, &usage);
        proceso_terminado(nombre_proceso, pid);

    } else {
        // Sin timeout: espera de forma normal
        printf(COLOR_KERNEL "[Kernel] Esperando a " ANSI_RESET "%s%s (PID %d)..." ANSI_RESET "\n",
               color_proceso(nombre_proceso), nombre_legible(nombre_proceso), pid);
        fflush(stdout);
        // 'wait4' sin WNOHANG (con 0) se bloquea hasta que el 'pid' termine.
        wait4(pid, &status, 0, &usage);
        proceso_terminado(nombre_proceso, pid);
    }

// Etiqueta 'goto' para centralizar dónde se guardan las estadísticas
guardar_stats:
    if (strstr(nombre_proceso, "proceso1")) { usage_p1 = usage; pid_p1 = pid; }
    else if (strstr(nombre_proceso, "proceso2")) { usage_p2 = usage; pid_p2 = pid; }
    else if (strstr(nombre_proceso, "proceso3")) { usage_p3 = usage; pid_p3 = pid; }
}

// Esta función se ejecuta *SOLO* en el proceso hijo
void lanzar_hijo_exec(char *const argv[]) {
    // 'execvp' REEMPLAZA el programa actual (el proceso hijo) por uno nuevo.
    // Carga el programa "qemu-riscv32" y le pasa los argumentos 'argv'.
    // 'v' = argumentos en un vector (array). 'p' = busca el ejecutable en el PATH.
    execvp("qemu-riscv32", argv);
    
    // Si 'execvp' tiene éxito, esta parte del código NUNCA se ejecuta.
    // Si llega aquí, es que 'execvp' falló (ej: no encontró "qemu-riscv32").
    perror(COLOR_ERROR "Error en execvp" ANSI_RESET); // Imprime el error
    exit(1); // Termina el proceso hijo con error
}

int leer_datos_p3(int pipe_fd) {
    char buffer[128];
    // 'read' lee datos desde un 'descriptor de archivo' (en este caso, el pipe).
    ssize_t bytes = read(pipe_fd, buffer, sizeof(buffer) - 1);
    int valor = 0;

    if (bytes > 0) {
        buffer[bytes] = '\0'; // Asegura que el string esté terminado
        if (buffer[bytes - 1] == '\n') buffer[bytes - 1] = '\0'; // Quita el salto de línea
        printf(COLOR_KERNEL "[Kernel] Datos recibidos de " ANSI_RESET "%s%s" COLOR_KERNEL ": [%s]" ANSI_RESET "\n",
               color_proceso("./proceso3"), nombre_legible("./proceso3"), buffer);
        valor = atoi(buffer); // Convierte el texto a número
    } else {
        printf(COLOR_KERNEL "[Kernel] No se recibieron datos de " ANSI_RESET "%s%s" COLOR_KERNEL "." ANSI_RESET "\n",
               color_proceso("./proceso3"), nombre_legible("./proceso3"));
    }
    close(pipe_fd); // Cierra el extremo de lectura del pipe
    return valor;
}

// --- Escenarios de Ejecución ---

void ejecutar_escenario_1() {
    pid_t pid1, pid2, pid3;
    const int TIMEOUT_P3 = 5;
    
    // Array para los dos extremos del pipe: [0] es para LEER, [1] es para ESCRIBIR.
    int datos_pipe[2];
    // 'pipe' crea una tubería (un canal de comunicación unidireccional).
    if (pipe(datos_pipe) == -1) { perror(COLOR_ERROR "pipe datos" ANSI_RESET); exit(1); }

    // 'fork' crea un proceso hijo (una copia exacta del padre).
    // Si 'fork()' devuelve 0, significa que estamos en el proceso HIJO.
    // Si devuelve > 0, estamos en el PADRE (y el valor es el PID del hijo).
    if ((pid1 = fork()) == 0) {
        // Código del HIJO (Proceso 1)
        char *argv[] = {"qemu-riscv32", "./proceso1", NULL};
        lanzar_hijo_exec(argv);
    }
    printf(COLOR_KERNEL "[Kernel] Lanzando " ANSI_RESET "%s%s (PID %d)..." ANSI_RESET "\n", color_proceso("./proceso1"), nombre_legible("./proceso1"), pid1);

    if ((pid2 = fork()) == 0) {
        // Código del HIJO (Proceso 2)
        char *argv[] = {"qemu-riscv32", "./proceso2", NULL};
        lanzar_hijo_exec(argv);
    }
    printf(COLOR_KERNEL "[Kernel] Lanzando " ANSI_RESET "%s%s (PID %d)..." ANSI_RESET "\n", color_proceso("./proceso2"), nombre_legible("./proceso2"), pid2);
    
    if ((pid3 = fork()) == 0) {
        // Código del HIJO (Proceso 3)
        // El hijo 3 va a escribir, así que cierra el extremo de LECTURA del pipe.
        close(datos_pipe[0]);
        
        // 'dup2' redirige un descriptor de archivo.
        // Aquí, conecta la "Salida Estándar" (STDOUT_FILENO, la pantalla)
        // al extremo de ESCRITURA del pipe (datos_pipe[1]).
        // Ahora, todo lo que P3 imprima con printf irá al pipe.
        dup2(datos_pipe[1], STDOUT_FILENO);
        
        close(datos_pipe[1]); // Cierra el descriptor original (ya no lo necesita)
        
        char *argv[] = {"qemu-riscv32", "./proceso3", NULL};
        lanzar_hijo_exec(argv);
    }
    // Código del PADRE (Kernel)
    printf(COLOR_KERNEL "[Kernel] Lanzando " ANSI_RESET "%s%s (PID %d)..." ANSI_RESET "\n", color_proceso("./proceso3"), nombre_legible("./proceso3"), pid3);
    // El padre solo va a leer, así que cierra el extremo de ESCRITURA del pipe.
    close(datos_pipe[1]);

    printf(COLOR_KERNEL "[Kernel] Pausando " ANSI_RESET "%s%s" COLOR_KERNEL " y " ANSI_RESET "%s%s" ANSI_RESET "\n",
           color_proceso("./proceso2"), nombre_legible("./proceso2"),
           color_proceso("./proceso3"), nombre_legible("./proceso3"));
    // 'SIGSTOP' envía la señal de "pausa" (como Ctrl+Z en la terminal).
    kill(pid2, SIGSTOP); kill(pid3, SIGSTOP);

    esperar_proceso(pid1, "./proceso1", 0); // Espera a P1 (sin timeout)
    
    printf(COLOR_KERNEL "[Kernel] Reanudando " ANSI_RESET "%s%s" ANSI_RESET "\n", color_proceso("./proceso2"), nombre_legible("./proceso2"));
    // 'SIGCONT' envía la señal de "continuar" para reanudar un proceso pausado.
    kill(pid2, SIGCONT);
    
    esperar_proceso(pid2, "./proceso2", 0); // Espera a P2
    
    printf(COLOR_KERNEL "[Kernel] Reanudando " ANSI_RESET "%s%s" ANSI_RESET "\n", color_proceso("./proceso3"), nombre_legible("./proceso3"));
    kill(pid3, SIGCONT);
    
    esperar_proceso(pid3, "./proceso3", TIMEOUT_P3); // Espera a P3 (con timeout)

    // El padre lee lo que P3 escribió en el pipe
    leer_datos_p3(datos_pipe[0]);
}

void ejecutar_escenario_2() {
    pid_t pid1, pid2, pid3;
    // Dos pipes: uno para P1 -> P3 (uart) y otro para P3 -> Padre (datos)
    int uart_pipe[2], datos_pipe[2];
    if (pipe(uart_pipe) == -1 || pipe(datos_pipe) == -1) { perror(COLOR_ERROR "pipe" ANSI_RESET); exit(1); }

    if ((pid3 = fork()) == 0) {
        // Código del HIJO (Proceso 3)
        close(uart_pipe[1]); // P3 cierra escritura de uart
        // Redirige la "Entrada Estándar" (STDIN_FILENO, el teclado)
        // para que LEA desde el extremo de lectura del uart_pipe.
        dup2(uart_pipe[0], STDIN_FILENO);
        
        close(datos_pipe[0]); // P3 cierra lectura de datos
        // Redirige la "Salida Estándar" (STDOUT_FILENO, la pantalla)
        // para que ESCRIBA en el extremo de escritura del datos_pipe.
        dup2(datos_pipe[1], STDOUT_FILENO);
        
        char *argv[] = {"qemu-riscv32", "./proceso3", NULL};
        lanzar_hijo_exec(argv);
    }
    printf(COLOR_KERNEL "[Kernel] Lanzando " ANSI_RESET "%s%s (PID %d)..." ANSI_RESET "\n", color_proceso("./proceso3"), nombre_legible("./proceso3"), pid3);

    if ((pid1 = fork()) == 0) {
        // Código del HIJO (Proceso 1)
        close(datos_pipe[0]); close(datos_pipe[1]); // P1 no usa el pipe de datos
        
        close(uart_pipe[0]); // P1 cierra lectura de uart
        // Redirige STDOUT (pantalla) para que ESCRIBA en el uart_pipe.
        dup2(uart_pipe[1], STDOUT_FILENO);
        
        char *argv[] = {"qemu-riscv32", "./proceso1", NULL};
        lanzar_hijo_exec(argv);
    }
    printf(COLOR_KERNEL "[Kernel] Lanzando " ANSI_RESET "%s%s (PID %d)..." ANSI_RESET "\n", color_proceso("./proceso1"), nombre_legible("./proceso1"), pid1);

    // Código del PADRE (Kernel)
    // El padre cierra todos los extremos de los pipes que no usará.
    // Solo se queda con datos_pipe[0] para leer el resultado final de P3.
    close(uart_pipe[0]); close(uart_pipe[1]); close(datos_pipe[1]);
    
    kill(pid3, SIGSTOP); // Pausa P3 (esperando la salida de P1)

    esperar_proceso(pid1, "./proceso1", 0); // Espera a P1
    
    printf(COLOR_KERNEL "[Kernel] Reanudando " ANSI_RESET "%s%s" ANSI_RESET "\n", color_proceso("./proceso3"), nombre_legible("./proceso3"));
    kill(pid3, SIGCONT); // Reanuda P3 (que ahora puede leer la salida de P1)
    
    esperar_proceso(pid3, "./proceso3", 0);

    int temp_value = leer_datos_p3(datos_pipe[0]); // Lee el resultado de P3
    char *arg = (temp_value >= 100) ? "1" : "0"; // Decide el argumento para P2

    if ((pid2 = fork()) == 0) {
        // Pasa el 'arg' como argumento de línea de comandos a proceso2
        char *argv[] = {"qemu-riscv32", "./proceso2", arg, NULL};
        lanzar_hijo_exec(argv);
    }
    printf(COLOR_KERNEL "[Kernel] Lanzando " ANSI_RESET "%s%s (PID %d) con argumento: %s" ANSI_RESET "\n", color_proceso("./proceso2"), nombre_legible("./proceso2"), pid2, arg);
    esperar_proceso(pid2, "./proceso2", 0);
}

void ejecutar_escenario_3() {
    pid_t pid1, pid2, pid3;
    int uart_pipe[2], datos_pipe[2]; // Misma configuración de pipes que Escenario 2
    if (pipe(uart_pipe) == -1 || pipe(datos_pipe) == -1) { perror(COLOR_ERROR "pipe" ANSI_RESET); exit(1); }

    // Lanza P2 primero
    if ((pid2 = fork()) == 0) {
        char *argv[] = {"qemu-riscv32", "./proceso2", NULL};
        lanzar_hijo_exec(argv);
    }
    printf(COLOR_KERNEL "[Kernel] Lanzando " ANSI_RESET "%s%s (PID %d)..." ANSI_RESET "\n", color_proceso("./proceso2"), nombre_legible("./proceso2"), pid2);

    // Lanza P3 (leerá de uart_pipe, escribirá en datos_pipe)
    if ((pid3 = fork()) == 0) {
        close(uart_pipe[1]); dup2(uart_pipe[0], STDIN_FILENO);
        close(datos_pipe[0]); dup2(datos_pipe[1], STDOUT_FILENO);
        char *argv[] = {"qemu-riscv32", "./proceso3", NULL};
        lanzar_hijo_exec(argv);
    }
    printf(COLOR_KERNEL "[Kernel] Lanzando " ANSI_RESET "%s%s (PID %d)..." ANSI_RESET "\n", color_proceso("./proceso3"), nombre_legible("./proceso3"), pid3);

    // Lanza P1 (escribirá en uart_pipe)
    if ((pid1 = fork()) == 0) {
        close(datos_pipe[0]); close(datos_pipe[1]);
        close(uart_pipe[0]); dup2(uart_pipe[1], STDOUT_FILENO);
        char *argv[] = {"qemu-riscv32", "./proceso1", NULL};
        lanzar_hijo_exec(argv);
    }
    printf(COLOR_KERNEL "[Kernel] Lanzando " ANSI_RESET "%s%s (PID %d)..." ANSI_RESET "\n", color_proceso("./proceso1"), nombre_legible("./proceso1"), pid1);

    // Código del PADRE (Kernel)
    close(uart_pipe[0]); close(uart_pipe[1]); close(datos_pipe[1]);
    
    // Pausa P1 y P3, dejando que P2 corra primero
    kill(pid1, SIGSTOP); kill(pid3, SIGSTOP);

    esperar_proceso(pid2, "./proceso2", 0); // Espera a P2
    
    // Reanuda P1 y P3 en secuencia
    printf(COLOR_KERNEL "[Kernel] Reanudando " ANSI_RESET "%s%s" ANSI_RESET "\n", color_proceso("./proceso1"), nombre_legible("./proceso1"));
    kill(pid1, SIGCONT);
    
    esperar_proceso(pid1, "./proceso1", 0);
    
    printf(COLOR_KERNEL "[Kernel] Reanudando " ANSI_RESET "%s%s" ANSI_RESET "\n", color_proceso("./proceso3"), nombre_legible("./proceso3"));
    kill(pid3, SIGCONT);
    
    esperar_proceso(pid3, "./proceso3", 0);

    leer_datos_p3(datos_pipe[0]);
}

void ejecutar_escenario() {
    // 'switch' simple para seleccionar la función de escenario
    switch (escenario_actual) {
        case 1: printf(COLOR_CICLO "--- Escenario 1: [P1] -> [P2] -> [P3] ---\n" ANSI_RESET); ejecutar_escenario_1(); break;
        case 2: printf(COLOR_CICLO "--- Escenario 2: [(P1 + P3)] -> [P2] ---\n" ANSI_RESET); ejecutar_escenario_2(); break;
        case 3: printf(COLOR_CICLO "--- Escenario 3: [P2] -> [(P1 + P3)] ---\n" ANSI_RESET); ejecutar_escenario_3(); break;
        case 4: printf(COLOR_CICLO "--- Escenario 4: En desarrollo ---\n" ANSI_RESET); break;
        default: printf(COLOR_ERROR "[Kernel] Error: Escenario no válido." ANSI_RESET "\n"); break;
    }
}

// Esta es una "función manejadora de señal" (signal handler).
void reiniciar_escenario() {
    system("clear"); // Limpia la pantalla
    printf(COLOR_KERNEL "[Kernel] Reiniciando escenario..." ANSI_RESET "\n");
    escenario_actual = 0; // Vuelve a 0 para que pregunte de nuevo
    ciclo_actual = 1; // Reinicia el contador de ciclo
}

// --- Punto de Entrada Principal ---

int main() {
    
    printf(COLOR_KERNEL "[Kernel] Iniciando el Orquestador de Satélite." ANSI_RESET "\n");
    printf("El ciclo se repetirá cada 10 segundos - Ctrl + Z para reiniciar escenario.\n");
    
    // 'signal' intercepta una señal del sistema operativo.
    // Aquí, "atrapa" la señal SIGTSTP (la que se genera al presionar Ctrl+Z)
    // y en lugar de la acción por defecto (pausar), ejecuta la función 'reiniciar_escenario'.
    signal(SIGTSTP, reiniciar_escenario);

    while (1) { // Bucle infinito
        printf(COLOR_CICLO "\n--- Inicio del ciclo #%d ---\n" ANSI_RESET, ciclo_actual);
        inicializar_ciclo(); // Limpia las estadísticas

        if (escenario_actual == 0) {
            printf("Seleccione el escenario de ejecución (1-4): ");
            if (scanf("%d", &escenario_actual) != 1 || escenario_actual < 1 || escenario_actual > 4)
                escenario_actual = 0; // Resetea si la entrada es inválida
            
            // Este bucle 'limpia' el buffer de entrada.
            // Después de que el usuario presiona (ej: '1' + 'Enter'), 'scanf' lee el '1'
            // pero deja el '\n' (Enter) en el buffer. Este bucle consume ese '\n'
            // para evitar problemas en futuras lecturas.
            while (fgetc(stdin) != '\n'); 
        }

        if (escenario_actual != 0) {
            printf(COLOR_KERNEL "\n[Kernel] Ejecutando escenario: %d..." ANSI_RESET "\n", escenario_actual);
            ejecutar_escenario();
        }

        mostrar_tabla_recursos(); // Imprime la tabla de estadísticas
        printf(COLOR_CICLO "--- Fin de ciclo #%d ---\n" ANSI_RESET, ciclo_actual++);
        
        // 'sleep' pausa la ejecución del 'main' (el orquestador) por 10 segundos
        // antes de iniciar el siguiente ciclo.
        sleep(10);
    }
    return 0; // Esta línea nunca se alcanza
}