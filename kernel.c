#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>

// Definiciones de códigos ANSI para colorear la salida en la terminal.
// Esto mejora la legibilidad de la simulación.
#define ANSI_RESET "\x1B[0m"
#define ANSI_RED "\x1B[31m"
#define ANSI_GREEN "\x1B[32m"
#define ANSI_YELLOW "\x1B[33m"
#define ANSI_BLUE "\x1B[34m"
#define ANSI_MAGENTA "\x1B[35m"
#define ANSI_CYAN "\x1B[36m"
#define ANSI_WHITE "\x1B[37m"
#define ANSI_BR_RED "\x1B[91m"
#define ANSI_BR_GREEN "\x1B[92m"
#define ANSI_BR_YELLOW "\x1B[93m"
#define ANSI_BR_CYAN "\x1B[96m"

// Definiciones de colores específicos para los componentes del sistema
#define COLOR_KERNEL ANSI_CYAN     // Color para mensajes del Orquestador (Control Central)
#define COLOR_YELLOW ANSI_YELLOW   // Color Amarillo
#define COLOR_P1 ANSI_BR_GREEN     // Color para Proceso 1 (Receptor de Señal)
#define COLOR_P2 ANSI_YELLOW       // Color para Proceso 2 (Control Escudo)
#define COLOR_P3 ANSI_BLUE         // Color para Proceso 3 (Analizador Espectral)
#define COLOR_ERROR ANSI_BR_RED    // Color para errores
#define COLOR_TABLE ANSI_WHITE     // Color para la tabla de métricas
#define COLOR_CICLO ANSI_BR_YELLOW // Color para el inicio/fin de cada ciclo

// Variables estáticas globales para el estado del orquestador.
static int escenario_actual = 0; // Almacena el número del protocolo (escenario) a ejecutar (1, 2, 3, etc.)
static int ciclo_actual = 1;     // Contador para el número de ciclos de ejecución completados.

// Estructuras para almacenar las métricas de uso de recursos (CPU, memoria, etc.) de cada proceso hijo.
// 'rusage' es una estructura estándar de Unix para estadísticas de recursos.
static struct rusage usage_p1, usage_p2, usage_p3;
// PIDs (Process IDs) de los procesos hijo para poder manipularlos (señales, espera) y reportar métricas.
static pid_t pid_p1 = 0, pid_p2 = 0, pid_p3 = 0;
// Estructuras 'timeval' para registrar el tiempo de inicio de cada proceso y del ciclo completo.
static struct timeval p1_start, p2_start, p3_start, ciclo_start;
// Almacena el tiempo real (wall time) que tardó cada proceso en ejecutarse.
static double time_p1 = 0, time_p2 = 0, time_p3 = 0;
// Almacena el tiempo total del Escenario 1 (Secuencial) para calcular el SpeedUp en otros escenarios.
static double tiempo_escenario_1 = 0.0;

/**
 * @brief Guarda las estadísticas de uso de recursos de un proceso terminado.
 * * @param nombre_proceso Nombre del ejecutable (ej: "./proceso1"). Se usa para identificar P1, P2 o P3.
 * @param pid PID del proceso terminado.
 * @param wall_time Tiempo real transcurrido.
 * @param usage Estructura rusage con las métricas de recursos.
 */
void guardar_stats_proceso(const char *nombre_proceso, pid_t pid, double wall_time, struct rusage *usage)
{
    // Identifica qué proceso terminó y guarda sus datos en las variables globales correspondientes.
    if (strstr(nombre_proceso, "proceso1"))
    {
        usage_p1 = *usage;
        pid_p1 = pid;
        time_p1 = wall_time;
    }
    else if (strstr(nombre_proceso, "proceso2"))
    {
        usage_p2 = *usage;
        pid_p2 = pid;
        time_p2 = wall_time;
    }
    else if (strstr(nombre_proceso, "proceso3"))
    {
        usage_p3 = *usage;
        pid_p3 = pid;
        time_p3 = wall_time;
    }
}

/**
 * @brief Calcula la diferencia de tiempo en segundos entre dos estructuras timeval.
 * * @param start Tiempo de inicio.
 * @param end Tiempo de fin.
 * @return double La diferencia en segundos.
 */
double timeval_diff(struct timeval *start, struct timeval *end)
{
    // Fórmula: (segundos de fin - segundos de inicio) + (microsegundos de fin - microsegundos de inicio) / 1,000,000
    return (end->tv_sec - start->tv_sec) +
           (end->tv_usec - start->tv_usec) / 1000000.0;
}

/**
 * @brief Reinicia las variables de seguimiento de PIDs y métricas al inicio de cada ciclo.
 */
void inicializar_ciclo()
{
    // Reinicia los PIDs para saber qué procesos se ejecutaron en este ciclo.
    pid_p1 = pid_p2 = pid_p3 = 0;
    // Reinicia el tiempo real de ejecución.
    time_p1 = time_p2 = time_p3 = 0.0;

    // Inicializa las estructuras rusage a cero.
    memset(&usage_p1, 0, sizeof(struct rusage));
    memset(&usage_p2, 0, sizeof(struct rusage));
    memset(&usage_p3, 0, sizeof(struct rusage));
}

/**
 * @brief Devuelve un nombre descriptivo para cada proceso (para la salida legible).
 * * @param nombre El nombre del ejecutable (ej: "./proceso1").
 * @return const char* El nombre legible (ej: "Receptor de Señal").
 */
const char *nombre_legible(const char *nombre)
{
    if (strstr(nombre, "proceso1"))
        return "Receptor de Señal";
    if (strstr(nombre, "proceso2"))
        return "Control Escudo";
    if (strstr(nombre, "proceso3"))
        return "Analizador Espectral";
    return nombre;
}

/**
 * @brief Devuelve el código de color ANSI para un proceso.
 * * @param nombre El nombre del ejecutable.
 * @return const char* El código de color.
 */
const char *color_proceso(const char *nombre)
{
    if (strstr(nombre, "proceso1"))
        return COLOR_P1;
    if (strstr(nombre, "proceso2"))
        return COLOR_P2;
    if (strstr(nombre, "proceso3"))
        return COLOR_P3;
    return ANSI_WHITE;
}

/**
 * @brief Imprime un mensaje estándar cuando un proceso hijo ha terminado.
 * * @param nombre_proceso Nombre del ejecutable.
 * @param pid PID del proceso.
 */
void proceso_terminado(const char *nombre_proceso, pid_t pid)
{
    printf("\n" COLOR_KERNEL "[Control Central] " ANSI_RESET "%s%s (PID %d) ha completado su tarea." ANSI_RESET "\n",
           color_proceso(nombre_proceso), nombre_legible(nombre_proceso), pid);

    fflush(stdout);
}

/**
 * @brief Imprime una fila con las métricas de un proceso en la tabla de recursos.
 * * @param nombre Nombre del ejecutable.
 * @param pid PID del proceso.
 * @param wall_time Tiempo real transcurrido.
 * @param usage Estructura rusage con las métricas de recursos.
 */
void print_fila_tabla(const char *nombre, pid_t pid, double wall_time, struct rusage *usage)
{
    // Formatea e imprime los datos de la estructura rusage:
    // T. Real (wall_time), CPU Usuario (ru_utime), CPU Sistema (ru_stime), Memoria Pico (ru_maxrss).
    printf("%s| %-20s | %-7d | %-11.6f | %ld.%06ld s | %ld.%06ld s | %-15ld |" ANSI_RESET "\n",
           color_proceso(nombre),
           nombre_legible(nombre), pid,
           wall_time,
           usage->ru_utime.tv_sec, usage->ru_utime.tv_usec,
           usage->ru_stime.tv_sec, usage->ru_stime.tv_usec,
           usage->ru_maxrss); // ru_maxrss es la memoria física máxima usada (kilobytes)
}

/**
 * @brief Imprime la tabla final de recursos para el ciclo actual, usando los datos guardados.
 */
void mostrar_tabla_recursos()
{
    printf(COLOR_TABLE "\n--- Resumen de Recursos del Ciclo ---\n");

    printf("| Proceso              | PID     | T. Real (s) | CPU Usuario | CPU Sistema | Memoria Pico (KB) |\n");
    printf("|----------------------|---------|-------------|-------------|-------------|-------------------|\n" ANSI_RESET);

    // Solo imprime la fila si el proceso se ejecutó (pid > 0).
    if (pid_p1 > 0)
        print_fila_tabla("./proceso1", pid_p1, time_p1, &usage_p1);
    if (pid_p2 > 0)
        print_fila_tabla("./proceso2", pid_p2, time_p2, &usage_p2);
    if (pid_p3 > 0)
        print_fila_tabla("./proceso3", pid_p3, time_p3, &usage_p3);

    printf(COLOR_TABLE "|----------------------|---------|-------------|-------------|-------------|-------------------|\n" ANSI_RESET);
    fflush(stdout);
}

/**
 * @brief Espera la terminación de un proceso hijo, manejando un posible timeout y recolectando métricas.
 * * @param pid PID del proceso hijo a esperar.
 * @param nombre_proceso Nombre del ejecutable.
 * @param timeout_sec Tiempo máximo en segundos para esperar (0 para espera indefinida/bloqueante).
 * @param start_time Estructura timeval del tiempo de inicio del proceso.
 */
void esperar_proceso(pid_t pid, const char *nombre_proceso, int timeout_sec, struct timeval *start_time)
{
    int status;
    struct rusage usage;
    struct timeval end_time;
    double wall_time = 0.0;

    if (timeout_sec > 0)
    {
        // Bloque de espera con timeout (usando wait4 y WNOHANG)
        for (int remaining = timeout_sec; remaining > 0; remaining--)
        {
            // wait4 con WNOHANG: No bloquea, regresa inmediatamente si el hijo no ha terminado.
            pid_t terminado = wait4(pid, &status, WNOHANG, &usage);

            if (terminado == pid)
            {
                // El proceso terminó dentro del tiempo.
                gettimeofday(&end_time, NULL);
                wall_time = timeval_diff(start_time, &end_time);
                proceso_terminado(nombre_proceso, pid);
                goto guardar_stats; // Salta a guardar las métricas
            }
            sleep(1); // Espera 1 segundo antes de reintentar.
        }

        // Si el bucle termina, se agotó el timeout.
        printf(COLOR_ERROR "[Control Central] ALERTA: " ANSI_RESET "%s%s (PID %d) no responde. Forzando terminación (SIGKILL)..." ANSI_RESET "\n",
               color_proceso(nombre_proceso), nombre_legible(nombre_proceso), pid);
        kill(pid, SIGKILL);             // Envía una señal SIGKILL para terminarlo forzosamente.
        wait4(pid, &status, 0, &usage); // Espera la terminación forzosa.
        gettimeofday(&end_time, NULL);
        wall_time = timeval_diff(start_time, &end_time);
        proceso_terminado(nombre_proceso, pid);
    }
    else
    {
        // Bloque de espera sin timeout (espera bloqueante normal).
        printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Esperando finalización de %s%s (PID %d)..." ANSI_RESET "\n",
               color_proceso(nombre_proceso), nombre_legible(nombre_proceso), pid);
        fflush(stdout);
        wait4(pid, &status, 0, &usage); // Espera a que el proceso termine, bloqueando al padre.
        gettimeofday(&end_time, NULL);
        wall_time = timeval_diff(start_time, &end_time);
        proceso_terminado(nombre_proceso, pid);
    }

guardar_stats:
    // Guarda las estadísticas recolectadas (ya sea por finalización normal o por timeout/SIGKILL).
    guardar_stats_proceso(nombre_proceso, pid, wall_time, &usage);
}

/**
 * @brief Función que se ejecuta en el proceso hijo para reemplazar su código con el ejecutable real.
 * * Usa execvp para iniciar el componente de software (simulado con qemu-riscv32).
 * * @param argv Argumentos para el comando execvp (ej: "qemu-riscv32", "./proceso1", NULL).
 */
void lanzar_hijo_exec(char *const argv[])
{
    // execvp reemplaza la imagen del proceso actual (el hijo recién creado) con el nuevo programa.
    // Si execvp tiene éxito, nunca regresa; solo regresa si hay un error.
    execvp("qemu-riscv32", argv);
    // Si llegamos aquí, hubo un error en execvp.
    perror(COLOR_ERROR "Error al iniciar componente de software" ANSI_RESET);
    exit(1);
}

/**
 * @brief Lee los datos finales de 'proceso3' (Analizador Espectral) desde un pipe.
 * * @param pipe_fd Descriptor de archivo de lectura del pipe.
 * @return int El valor numérico leído (temperatura, etc.) o 0 si no hay datos.
 */
int leer_datos_p3(int pipe_fd)
{
    char buffer[128];
    // Lee hasta 127 bytes del pipe.
    ssize_t bytes = read(pipe_fd, buffer, sizeof(buffer) - 1);
    int valor = 0;

    if (bytes > 0)
    {
        // Asegura que la cadena esté terminada en nulo.
        buffer[bytes] = '\0';
        // Elimina el salto de línea si existe.
        if (buffer[bytes - 1] == '\n')
            buffer[bytes - 1] = '\0';

        printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Análisis recibido de %s%s" COLOR_KERNEL ": [%s]" ANSI_RESET "\n",
               color_proceso("./proceso3"), nombre_legible("./proceso3"), buffer);
        // Convierte la cadena leída a un entero.
        valor = atoi(buffer);
    }
    else
    {
        printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "No se recibió análisis final de %s%s" COLOR_KERNEL "." ANSI_RESET "\n",
               color_proceso("./proceso3"), nombre_legible("./proceso3"));
    }

    close(pipe_fd); // Cierra el extremo de lectura.
    return valor;
}

/**
 * @brief Envía el contenido de un archivo (simulando datos de telemetría) a un pipe.
 * * @param pipe_fd_escritura Descriptor de archivo de escritura del pipe.
 * @param archivo Nombre del archivo a leer (ej: "medidas.txt").
 */
void enviar_contenido_archivo_a_pipe(int pipe_fd_escritura, const char *archivo)
{
    FILE *fp = fopen(archivo, "r");
    if (!fp)
    {
        fprintf(stderr, COLOR_ERROR "[Control Central] ERROR: No se puede abrir el archivo de señal %s\n" ANSI_RESET, archivo);
        return;
    }

    char datos_leidos;
    // Lee carácter por carácter del archivo y lo escribe en el pipe.
    while ((datos_leidos = fgetc(fp)) != EOF)
    {
        write(pipe_fd_escritura, &datos_leidos, 1);
    }

    fclose(fp);
}

/**
 * @brief Implementa el Escenario 1: Ejecución Secuencial.
 * Orden: Proceso 1 (Receptor) -> Proceso 2 (Escudo) -> Proceso 3 (Analizador).
 * P1 y P3 están conectados por un pipe.
 * P2 y P3 se inician pausados (SIGSTOP) y se activan secuencialmente.
 */
void ejecutar_escenario_1()
{
    pid_t pid1, pid2, pid3;
    const int TIMEOUT_P3 = 5; // Timeout para P3 (Analizador Espectral).

    // Conexiones de Pipe (Comunicación entre Procesos):
    int datos_pipe_p3[2]; // Pipe para que P3 envíe su resultado final al Orquestador (Kernel).
    int p1_input_pipe[2]; // Pipe para que el Orquestador envíe datos de 'medidas.txt' al STDIN de P1.
    int p1_to_p3_pipe[2]; // Pipe para que P1 envíe su salida (STDOUT) al STDIN de P3.

    // Creación de los Pipes:
    if (pipe(datos_pipe_p3) == -1)
    {
        perror(COLOR_ERROR "pipe datos_pipe_p3" ANSI_RESET);
        exit(1);
    }
    if (pipe(p1_input_pipe) == -1)
    {
        perror(COLOR_ERROR "pipe p1_input_pipe" ANSI_RESET);
        exit(1);
    }
    if (pipe(p1_to_p3_pipe) == -1)
    {
        perror(COLOR_ERROR "pipe p1_to_p3_pipe" ANSI_RESET);
        exit(1);
    }

    // --- 1. Lanzamiento de Proceso 1 (Receptor de Señal) ---
    gettimeofday(&p1_start, NULL); // Tiempo de inicio de P1
    if ((pid1 = fork()) == 0)      // Proceso Hijo P1
    {
        // Redirección de STDIN (entrada):
        // Cierra el extremo de escritura del pipe de entrada, redirige el extremo de lectura a STDIN.
        close(p1_input_pipe[1]);
        dup2(p1_input_pipe[0], STDIN_FILENO);
        close(p1_input_pipe[0]);

        // Redirección de STDOUT (salida):
        // Cierra el extremo de lectura del pipe P1->P3, redirige el extremo de escritura a STDOUT.
        close(p1_to_p3_pipe[0]);
        dup2(p1_to_p3_pipe[1], STDOUT_FILENO);
        close(p1_to_p3_pipe[1]);

        // Cierra los pipes no utilizados.
        close(datos_pipe_p3[0]);
        close(datos_pipe_p3[1]);

        char *argv[] = {"qemu-riscv32", "./code/escenariosBasicos/proceso1", NULL};
        lanzar_hijo_exec(argv); // El hijo se convierte en P1.
    }

    // Proceso Padre (Orquestador): Cierra los extremos de pipe que no va a usar.
    close(p1_input_pipe[0]); // El padre solo escribe la entrada a P1.
    close(p1_to_p3_pipe[1]); // El padre solo lee de P1 (a través de P3), pero en este escenario solo P3 lee.

    // El Orquestador envía los datos de 'medidas.txt' al STDIN de P1.
    enviar_contenido_archivo_a_pipe(p1_input_pipe[1], "medidas.txt");
    close(p1_input_pipe[1]); // Cierra la escritura de P1, indicando fin de datos (EOF).

    // --- 2. Lanzamiento de Proceso 2 (Control Escudo) ---
    if ((pid2 = fork()) == 0) // Proceso Hijo P2
    {
        // Cierra todos los pipes ya que P2 no usa ninguno en este escenario (solo se activa y termina).
        close(datos_pipe_p3[0]);
        close(datos_pipe_p3[1]);
        close(p1_input_pipe[0]);
        close(p1_input_pipe[1]);
        close(p1_to_p3_pipe[0]);
        close(p1_to_p3_pipe[1]);

        char *argv[] = {"qemu-riscv32", "./code/escenariosBasicos/proceso2", NULL};
        lanzar_hijo_exec(argv); // El hijo se convierte en P2.
    }

    // --- 3. Lanzamiento de Proceso 3 (Analizador Espectral) ---
    if ((pid3 = fork()) == 0) // Proceso Hijo P3
    {
        // Redirección de STDOUT:
        // Cierra la lectura del pipe P3->Kernel, redirige la escritura a STDOUT.
        close(datos_pipe_p3[0]);
        dup2(datos_pipe_p3[1], STDOUT_FILENO);
        close(datos_pipe_p3[1]);

        // Redirección de STDIN:
        // Cierra la escritura del pipe P1->P3, redirige la lectura a STDIN.
        close(p1_to_p3_pipe[1]);
        dup2(p1_to_p3_pipe[0], STDIN_FILENO);
        close(p1_to_p3_pipe[0]);

        // Cierra los pipes no utilizados.
        close(p1_input_pipe[0]);
        close(p1_input_pipe[1]);

        char *argv[] = {"qemu-riscv32", "./code/escenariosBasicos/proceso3", NULL};
        lanzar_hijo_exec(argv); // El hijo se convierte en P3.
    }

    // Proceso Padre (Orquestador): Cierra los extremos de pipe que no va a usar.
    close(datos_pipe_p3[1]); // El padre solo lee el resultado final de P3.
    close(p1_to_p3_pipe[0]); // El padre no usa el pipe P1->P3.

    // --- 4. Sincronización Secuencial (SIGSTOP / SIGCONT) ---
    // El Orquestador pausa P2 y P3 al inicio.
    kill(pid2, SIGSTOP);
    kill(pid3, SIGSTOP);

    // Espera a que P1 termine (espera bloqueante: 0 timeout). P1 procesa los datos de medidas.txt.
    esperar_proceso(pid1, "./code/escenariosBasicos/proceso1", 0, &p1_start);

    // P1 terminó. Activa P2.
    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Receptor (P1) terminó. Activando %s%s" COLOR_KERNEL "..." ANSI_RESET "\n", color_proceso("proceso2"), nombre_legible("proceso2"));

    // Registrar tiempo de inicio de P2 justo antes de activarlo.
    gettimeofday(&p2_start, NULL);
    kill(pid2, SIGCONT);

    // Espera a que P2 termine.
    esperar_proceso(pid2, "./code/escenariosBasicos/proceso2", 0, &p2_start);

    // P2 terminó. Activa P3.
    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Escudo (P2) terminó. Activando %s%s" COLOR_KERNEL "..." ANSI_RESET "\n", color_proceso("proceso3"), nombre_legible("proceso3"));

    // Registrar tiempo de inicio de P3 justo antes de activarlo.
    gettimeofday(&p3_start, NULL);
    kill(pid3, SIGCONT);

    // Espera a que P3 termine, con un timeout de 5 segundos.
    esperar_proceso(pid3, "./code/escenariosBasicos/proceso3", TIMEOUT_P3, &p3_start);

    // Lee el resultado final de P3 (el valor analizado) del pipe.
    leer_datos_p3(datos_pipe_p3[0]);
}

/**
 * @brief Implementa el Escenario 2: Ejecución Concurrente.
 * P1 (Receptor) y P3 (Analizador) se ejecutan concurrentemente, compartiendo CPU por turnos (tiempo asignado).
 * P2 (Escudo) se lanza solo si P3 detecta una anomalía de temperatura.
 * Se usa fcntl para hacer el pipe P3->Kernel no bloqueante, permitiendo la lectura sin esperar datos.
 */
void ejecutar_escenario_2()
{
    pid_t pid1, pid2, pid3;
    const int TIMEOUT_P1 = 10; // Tiempo asignado a P1 antes de ser pausado.
    const int TIMEOUT_P3 = 5;  // Tiempo asignado a P3 antes de ser pausado.

    // Conexiones de Pipe:
    int p1_input_pipe[2];     // Orquestador -> STDIN de P1
    int p1_to_p3_pipe[2];     // STDOUT de P1 -> STDIN de P3
    int p3_to_kernel_pipe[2]; // STDOUT de P3 -> Orquestador (para monitoreo constante)

    // Estructuras de rusage locales para recopilar métricas durante los wait4 con WNOHANG.
    struct rusage local_usage_p1, local_usage_p3;
    memset(&local_usage_p1, 0, sizeof(struct rusage));
    memset(&local_usage_p3, 0, sizeof(struct rusage));

    // Creación de Pipes:
    if (pipe(p1_input_pipe) == -1)
    {
        perror(COLOR_ERROR "pipe p1_input_pipe" ANSI_RESET);
        exit(1);
    }
    if (pipe(p1_to_p3_pipe) == -1)
    {
        perror(COLOR_ERROR "pipe p1_to_p3_pipe" ANSI_RESET);
        exit(1);
    }
    if (pipe(p3_to_kernel_pipe) == -1)
    {
        perror(COLOR_ERROR "pipe p3_to_kernel_pipe" ANSI_RESET);
        exit(1);
    }

    // --- 1. Lanzamiento de Proceso 1 (Receptor de Señal) ---
    gettimeofday(&p1_start, NULL);
    if ((pid1 = fork()) == 0) // Proceso Hijo P1
    {
        // Redirección de STDIN (entrada): Orquestador -> P1
        close(p1_input_pipe[1]);
        dup2(p1_input_pipe[0], STDIN_FILENO);
        close(p1_input_pipe[0]);

        // Redirección de STDOUT (salida): P1 -> P3
        close(p1_to_p3_pipe[0]);
        dup2(p1_to_p3_pipe[1], STDOUT_FILENO);
        close(p1_to_p3_pipe[1]);

        // Cierra pipes no utilizados.
        close(p3_to_kernel_pipe[0]);
        close(p3_to_kernel_pipe[1]);

        char *argv[] = {"qemu-riscv32", "./code/escenariosBasicos/proceso1", NULL};
        lanzar_hijo_exec(argv);
    }

    // --- 2. Lanzamiento de Proceso 3 (Analizador Espectral) ---
    gettimeofday(&p3_start, NULL);
    if ((pid3 = fork()) == 0) // Proceso Hijo P3
    {
        // Cierra pipes no utilizados.
        close(p1_input_pipe[0]);
        close(p1_input_pipe[1]);

        // Redirección de STDIN (entrada): P1 -> P3
        close(p1_to_p3_pipe[1]);
        dup2(p1_to_p3_pipe[0], STDIN_FILENO);
        close(p1_to_p3_pipe[0]);

        // Redirección de STDOUT (salida): P3 -> Orquestador
        close(p3_to_kernel_pipe[0]);
        dup2(p3_to_kernel_pipe[1], STDOUT_FILENO);
        close(p3_to_kernel_pipe[1]);

        char *argv[] = {"qemu-riscv32", "./code/escenariosBasicos/proceso3", NULL};
        lanzar_hijo_exec(argv);
    }

    // Proceso Padre (Orquestador): Cierra los extremos de pipe que no va a usar.
    close(p1_input_pipe[0]);     // Solo escribe a P1.
    close(p1_to_p3_pipe[0]);     // P1 y P3 manejan la conexión.
    close(p1_to_p3_pipe[1]);     // P1 y P3 manejan la conexión.
    close(p3_to_kernel_pipe[1]); // Solo lee de P3.

    // Configura el pipe de P3 a Kernel en modo NO BLOQUEANTE.
    if (fcntl(p3_to_kernel_pipe[0], F_SETFL, O_NONBLOCK) < 0)
    {
        perror(COLOR_ERROR "fcntl O_NONBLOCK" ANSI_RESET);
        exit(1);
    }

    // Pausa P3 al inicio. P1 comienza la recepción.
    kill(pid3, SIGSTOP);
    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Pausando %s%s (PID %d) al inicio.\n", color_proceso("proceso3"), nombre_legible("proceso3"), pid3);

    // Envía los datos de entrada a P1.
    enviar_contenido_archivo_a_pipe(p1_input_pipe[1], "medidas.txt");
    close(p1_input_pipe[1]); // Cierra la escritura de P1.

    int p1_status, p3_status;
    int p1_sigue_vivo = 1;
    int p3_sigue_vivo = 1;
    int last_data_from_p3 = -1; // Almacena el último valor leído de P3.

    // --- Bucle principal de concurrencia ---
    // Repite hasta que P1 y P3 hayan terminado.
    while (p1_sigue_vivo || p3_sigue_vivo)
    {
        // --- 3. Ejecución de P1 (Receptor) ---
        if (p1_sigue_vivo)
        {
            // Asigna un quantum de tiempo a P1 (SIGCONT lo reanuda).
            kill(pid1, SIGCONT);
            sleep(TIMEOUT_P1); // Permite que P1 se ejecute por el tiempo asignado.

            // Revisa si P1 terminó durante el quantum (WNOHANG: no espera si no ha terminado).
            pid_t p1_terminado = wait4(pid1, &p1_status, WNOHANG, &local_usage_p1);
            if (p1_terminado == pid1)
            {
                // P1 terminó su tarea (leyó todo 'medidas.txt').
                p1_sigue_vivo = 0;

                // Guardar las estadísticas finales de P1 inmediatamente.
                struct timeval end_time_p1;
                gettimeofday(&end_time_p1, NULL);
                double wall_time_p1 = timeval_diff(&p1_start, &end_time_p1);
                guardar_stats_proceso("./code/escenariosBasicos/proceso1", pid1, wall_time_p1, &local_usage_p1);

                // Si P3 sigue vivo, le permite finalizar su procesamiento.
                if (p3_sigue_vivo)
                {
                    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Recepción terminada. Permitiendo al %s%s" COLOR_KERNEL " finalizar análisis..." ANSI_RESET "\n", color_proceso("proceso3"), nombre_legible("proceso3"));
                    kill(pid3, SIGCONT); // Activa P3 para el final.

                    // Lee todos los datos restantes del pipe de P3 antes de la espera bloqueante.
                    char buf[1024];
                    ssize_t bytes;
                    while ((bytes = read(p3_to_kernel_pipe[0], buf, 1023)) > 0)
                    {
                        buf[bytes] = '\0';
                        printf(COLOR_P3 "[Datos del Analizador]: \n%s" ANSI_RESET, buf);
                        fflush(stdout);
                    }

                    // Espera la terminación de P3 (espera bloqueante).
                    wait4(pid3, &p3_status, 0, &local_usage_p3);

                    // Guarda las estadísticas finales de P3.
                    struct timeval end_time_p3;
                    gettimeofday(&end_time_p3, NULL);
                    double wall_time_p3 = timeval_diff(&p3_start, &end_time_p3);
                    guardar_stats_proceso("./code/escenariosBasicos/proceso3", pid3, wall_time_p3, &local_usage_p3);

                    p3_sigue_vivo = 0;
                }
            }
            else
            {
                // P1 no terminó en el tiempo asignado. Pausa P1 (SIGSTOP).
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Tiempo de CPU agotado. Pausando %s%s (PID %d)\n", color_proceso("proceso1"), nombre_legible("proceso1"), pid1);
                kill(pid1, SIGSTOP);
            }
        }

        // --- 4. Ejecución de P3 (Analizador) ---
        // Solo ejecuta P3 si ambos procesos siguen vivos.
        if (p3_sigue_vivo && p1_sigue_vivo)
        {
            // Asigna un quantum de tiempo a P3.
            kill(pid3, SIGCONT);
            sleep(TIMEOUT_P3); // Permite que P3 se ejecute por el tiempo asignado.

            // Intenta leer la salida de P3 (no bloqueante, gracias a fcntl).
            char read_buf[1024];
            ssize_t bytes_read = read(p3_to_kernel_pipe[0], read_buf, 1023);
            if (bytes_read > 0)
            {
                read_buf[bytes_read] = '\0';
                printf(COLOR_P3 "[Datos del Analizador]: \n%s" ANSI_RESET, read_buf);
                fflush(stdout);

                // Lógica para extraer el último número (temperatura) del análisis de P3.
                char *last_num_str = NULL;
                char *token = strtok(read_buf, " \n\r\t");
                while (token)
                {
                    if (strlen(token) > 0)
                        last_num_str = token;
                    token = strtok(NULL, " \n\r\t");
                }

                if (last_num_str)
                {
                    last_data_from_p3 = atoi(last_num_str);
                    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Última lectura de temperatura [P3]: %d\n", last_data_from_p3);
                }
            }

            // Revisa si P3 terminó (WNOHANG).
            pid_t p3_terminado = wait4(pid3, &p3_status, WNOHANG, &local_usage_p3);
            if (p3_terminado == pid3)
            {
                // P3 terminó.
                p3_sigue_vivo = 0;

                // Guardar las estadísticas finales de P3 inmediatamente.
                struct timeval end_time_p3;
                gettimeofday(&end_time_p3, NULL);
                double wall_time_p3 = timeval_diff(&p3_start, &end_time_p3);
                guardar_stats_proceso("./code/escenariosBasicos/proceso3", pid3, wall_time_p3, &local_usage_p3);

                // Si P1 sigue vivo, lo detiene forzosamente (ya no se necesitan más datos).
                if (p1_sigue_vivo)
                {
                    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Análisis (P3) terminado. Deteniendo %s%s (P1)." ANSI_RESET "\n", color_proceso("proceso1"), nombre_legible("proceso1"));
                    kill(pid1, SIGKILL);
                    wait4(pid1, &p1_status, 0, &local_usage_p1); // Espera la terminación forzosa.

                    // Guarda las estadísticas finales de P1.
                    struct timeval end_time_p1;
                    gettimeofday(&end_time_p1, NULL);
                    double wall_time_p1 = timeval_diff(&p1_start, &end_time_p1);
                    guardar_stats_proceso("./code/escenariosBasicos/proceso1", pid1, wall_time_p1, &local_usage_p1);

                    p1_sigue_vivo = 0;
                }
            }
            else
            {
                // P3 no terminó en el tiempo asignado. Pausa P3.
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Tiempo de CPU agotado. Pausando %s%s (PID %d)\n", color_proceso("proceso3"), nombre_legible("proceso3"), pid3);
                kill(pid3, SIGSTOP);
            }
        }
        else if (p3_sigue_vivo && !p1_sigue_vivo)
        {
            // Caso de limpieza: P1 ya terminó, solo queda esperar el fin de P3.
            if (wait4(pid3, &p3_status, WNOHANG, &local_usage_p3) == pid3)
            {
                // P3 terminó.
                p3_sigue_vivo = 0;
                // NOTA: Las estadísticas de P3 deberían haberse guardado en el bloque de P1 (si P1 terminó primero).
                // Pero se añade por seguridad si P3 terminó solo.
                struct timeval end_time_p3;
                gettimeofday(&end_time_p3, NULL);
                double wall_time_p3 = timeval_diff(&p3_start, &end_time_p3);
                guardar_stats_proceso("./code/escenariosBasicos/proceso3", pid3, wall_time_p3, &local_usage_p3);
            }
        }

        // --- 5. Lógica de Activación de Proceso 2 (Escudo) ---
        // Se ejecuta si P1 y P3 siguen vivos, permitiendo a P2 interrumpir la concurrencia.
        if (p1_sigue_vivo && p3_sigue_vivo)
        {
            char *p2_arg = NULL;
            printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Evaluando activación de escudo. Temperatura actual [P3]: %d\n", last_data_from_p3);

            // Determina si se requiere activar o desactivar el escudo según la temperatura (datos de P3).
            if (last_data_from_p3 > 90)
            {
                p2_arg = "1"; // Activar escudo
            }
            else if (last_data_from_p3 < 60 && last_data_from_p3 != -1)
            {
                p2_arg = "0"; // Desactivar escudo
            }

            if (p2_arg != NULL)
            {
                // Lanza P2 (Control Escudo) con el argumento necesario.
                gettimeofday(&p2_start, NULL);
                if ((pid2 = fork()) == 0) // Proceso Hijo P2
                {
                    // Cierra todos los pipes.
                    close(p1_input_pipe[1]);
                    close(p1_to_p3_pipe[0]);
                    close(p1_to_p3_pipe[1]);
                    close(p3_to_kernel_pipe[0]);

                    char *argv[] = {"qemu-riscv32", "./code/escenariosBasicos/proceso2", p2_arg, NULL};
                    lanzar_hijo_exec(argv);
                }

                // Espera la terminación de P2 (bloqueante).
                esperar_proceso(pid2, "./code/escenariosBasicos/proceso2", 0, &p2_start);
                last_data_from_p3 = -1; // Reinicia el valor de lectura para evitar re-lanzamiento inmediato de P2.
            }
            else
            {
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Temperatura (%d) en rango normal. %s%s" COLOR_KERNEL " no es requerido. Omitiendo.\n",
                       last_data_from_p3, color_proceso("proceso2"), nombre_legible("proceso2"));
            }
        }
    }

    close(p3_to_kernel_pipe[0]);
    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Ciclo de procesamiento concurrente finalizado.\n");
}

/**
 * @brief Función placeholder para el Escenario 3 (en desarrollo).
 */
void ejecutar_escenario_3()
{
    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Escenario 3 (Protocolo de Escudo Primero) en desarrollo..." ANSI_RESET "\n");
}

/**
 * @brief Llama a la función del escenario actual seleccionado.
 */
void ejecutar_escenario()
{
    switch (escenario_actual)
    {
    case 1:
        printf(COLOR_CICLO "--- Protocolo 1: Secuencial [Receptor] -> [Escudo] -> [Analizador] ---\n" ANSI_RESET);
        ejecutar_escenario_1();
        break;
    case 2:
        printf(COLOR_CICLO "--- Protocolo 2: Concurrente [Receptor] -> [Analizador] -> [Escudo] ---\n" ANSI_RESET);
        ejecutar_escenario_2();
        break;
    case 3:
        printf(COLOR_CICLO "--- Protocolo 3: Concurrente [Escudo] -> [Receptor] -> [Analizador] ---\n" ANSI_RESET);
        ejecutar_escenario_3();
        break;
    case 4:
        printf(COLOR_CICLO "--- Protocolo 4: En desarrollo ---\n" ANSI_RESET);
        break;
    default:
        printf(COLOR_ERROR "[Control Central] Error: Protocolo de ejecución no válido." ANSI_RESET "\n");
        break;
    }
}

/**
 * @brief Manejador de la señal SIGTSTP (Ctrl+Z).
 * Reinicia el orquestador y pide seleccionar un nuevo escenario.
 */
void reiniciar_escenario()
{
    system("clear"); // Limpia la pantalla para la nueva selección.
    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Reinicio de protocolos solicitado (SIGTSTP). Seleccione nuevo protocolo." ANSI_RESET "\n");
    escenario_actual = 0;
    ciclo_actual = 1;
}

/**
 * @brief Función principal del Orquestador (Kernel de la simulación).
 */
int main()
{
    printf(COLOR_KERNEL "[Centro de Control] Iniciando Orquestador de Misión." ANSI_RESET "\n");
    printf(COLOR_YELLOW "El ciclo de monitoreo se repetirá cada 10 segundos.\n Presione Ctrl + Z para reiniciar y seleccionar un nuevo protocolo" ANSI_RESET "\n");

    // Configura el manejador de señal para SIGTSTP (Ctrl+Z).
    signal(SIGTSTP, reiniciar_escenario);

    while (1) // Bucle principal infinito del Orquestador.
    {

        // Bloque de selección de escenario (se ejecuta solo si escenario_actual es 0).
        if (escenario_actual == 0)
        {
            printf("Seleccione el protocolo de ejecución (1-4): ");
            if (scanf("%d", &escenario_actual) != 1 || escenario_actual < 1 || escenario_actual > 4)
                escenario_actual = 0; // Si la entrada es inválida, se reinicia la selección.
            // Limpia el buffer de entrada para evitar problemas en la próxima lectura.
            while (fgetc(stdin) != '\n')
                ;
        }

        if (escenario_actual != 0)
        {
            printf(COLOR_CICLO "\n--- Inicio del ciclo #%d ---\n" ANSI_RESET, ciclo_actual);
            gettimeofday(&ciclo_start, NULL); // Registra el tiempo de inicio del ciclo.
            inicializar_ciclo();              // Limpia las métricas de procesos anteriores.
            printf(COLOR_KERNEL "\n[Control Central] " ANSI_RESET "Iniciando protocolo de escenario: %d..." ANSI_RESET "\n", escenario_actual);
            ejecutar_escenario(); // Ejecuta la lógica del escenario seleccionado.
        }

        mostrar_tabla_recursos(); // Imprime las métricas de rendimiento recolectadas.

        // Cálculo del tiempo total del ciclo.
        struct timeval ciclo_end;
        gettimeofday(&ciclo_end, NULL);
        double tiempo_total_ciclo = timeval_diff(&ciclo_start, &ciclo_end);
        printf(COLOR_CICLO "Tiempo total del ciclo: %.6f segundos\n" ANSI_RESET, tiempo_total_ciclo);

        // Lógica de cálculo de SpeedUp (mejora de rendimiento).
        if (escenario_actual == 1)
        {
            // Establece el tiempo de referencia (base) del protocolo secuencial.
            if (tiempo_escenario_1 == 0.0)
            {
                tiempo_escenario_1 = tiempo_total_ciclo;
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Tiempo base (Protocolo Secuencial 1) establecido: %.6f s\n" ANSI_RESET, tiempo_escenario_1);
            }
        }
        else if (escenario_actual > 1 && tiempo_escenario_1 > 0.0)
        {
            // Calcula y muestra el SpeedUp: (Tiempo Base) / (Tiempo Concurrente).
            double speedup = tiempo_escenario_1 / tiempo_total_ciclo;
            printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Mejora (SpeedUp) vs Protocolo 1: %.2fx\n" ANSI_RESET, speedup);
        }

        printf(COLOR_CICLO "--- Fin de ciclo #%d ---\n" ANSI_RESET, ciclo_actual++);
        sleep(10); // Pausa el bucle por 10 segundos antes de iniciar el siguiente ciclo.
    }
    return 0; // Código inalcanzable, pero por convención.
}