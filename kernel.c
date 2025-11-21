#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>

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

#define COLOR_KERNEL ANSI_CYAN
#define COLOR_YELLOW ANSI_YELLOW
#define COLOR_P1 ANSI_BR_GREEN
#define COLOR_P2 ANSI_YELLOW
#define COLOR_P3 ANSI_BLUE
#define COLOR_ERROR ANSI_BR_RED
#define COLOR_TABLE ANSI_WHITE
#define COLOR_CICLO ANSI_BR_YELLOW
#define COLOR_ACUMULADO ANSI_MAGENTA

typedef struct
{

    double time_real;
    long ru_utime_sec, ru_utime_usec;
    long ru_stime_sec, ru_stime_usec;
    long ru_maxrss;
    int exit_status;

    int num_pausas;
    double tiempo_pausado_total;
    double tiempo_ejecucion_efectiva;
    double quantum_dado_total;
    double quantum_usado_total;
    int cambios_contexto_voluntario;
    int cambios_contexto_involuntario;
    int seniales_recibidas[64];
} ProcesoStats;

typedef struct
{
    int ciclo;
    int escenario;
    double tiempo_total_ciclo;
    double speedup;
    double tiempo_muerto_kernel;
    ProcesoStats p1_stats;
    ProcesoStats p2_stats;
    ProcesoStats p3_stats;
} CicloResultado;

typedef struct
{
    int total_ciclos_acumulados;
    double tiempo_real_total;
    double cpu_usuario_total;
    double cpu_sistema_total;
    long memoria_pico_total_kb;
} AcumuladorMetricas;

#define CICLOS_POR_REPORTE 5

static CicloResultado resultados_ciclos[CICLOS_POR_REPORTE];
static int indice_resultados = 0;

static AcumuladorMetricas acumulador_global = {0};

static int escenario_actual = 0;
static int ciclo_actual = 1;

static struct rusage usage_p1, usage_p2, usage_p3;
static pid_t pid_p1 = 0, pid_p2 = 0, pid_p3 = 0;
static struct timeval p1_start, p2_start, p3_start, ciclo_start;
static double time_p1 = 0, time_p2 = 0, time_p3 = 0;
static double tiempo_escenario_2 = 0.0;
static ProcesoStats p1_full_stats, p2_full_stats, p3_full_stats;

void copiar_rusage_a_stats(struct rusage *usage, double wall_time, ProcesoStats *stats, int exit_status_code)
{
    stats->time_real = wall_time;
    stats->ru_utime_sec = usage->ru_utime.tv_sec;
    stats->ru_utime_usec = usage->ru_utime.tv_usec;
    stats->ru_stime_sec = usage->ru_stime.tv_sec;
    stats->ru_stime_usec = usage->ru_stime.tv_usec;
    stats->ru_maxrss = usage->ru_maxrss;
    stats->cambios_contexto_voluntario = usage->ru_nvcsw;
    stats->cambios_contexto_involuntario = usage->ru_nivcsw;

    stats->tiempo_ejecucion_efectiva = (double)stats->ru_utime_sec + (double)stats->ru_utime_usec / 1000000.0 +
                                       (double)stats->ru_stime_sec + (double)stats->ru_stime_usec / 1000000.0;
    stats->exit_status = exit_status_code;
}

void guardar_stats_proceso(const char *nombre_proceso, pid_t pid, double wall_time, struct rusage *usage, int status)
{
    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    if (strstr(nombre_proceso, "proceso1"))
    {
        usage_p1 = *usage;
        pid_p1 = pid;
        time_p1 = wall_time;
        copiar_rusage_a_stats(usage, wall_time, &p1_full_stats, exit_code);
    }
    else if (strstr(nombre_proceso, "proceso2"))
    {
        usage_p2 = *usage;
        pid_p2 = pid;
        time_p2 = wall_time;
        copiar_rusage_a_stats(usage, wall_time, &p2_full_stats, exit_code);
    }
    else if (strstr(nombre_proceso, "proceso3"))
    {
        usage_p3 = *usage;
        pid_p3 = pid;
        time_p3 = wall_time;
        copiar_rusage_a_stats(usage, wall_time, &p3_full_stats, exit_code);
    }
}

double timeval_diff(struct timeval *start, struct timeval *end)
{
    return (end->tv_sec - start->tv_sec) +
           (end->tv_usec - start->tv_usec) / 1000000.0;
}

void inicializar_stats(ProcesoStats *stats)
{
    memset(stats, 0, sizeof(ProcesoStats));
}

void inicializar_ciclo()
{
    pid_p1 = pid_p2 = pid_p3 = 0;
    time_p1 = time_p2 = time_p3 = 0.0;
    memset(&usage_p1, 0, sizeof(struct rusage));
    memset(&usage_p2, 0, sizeof(struct rusage));
    memset(&usage_p3, 0, sizeof(struct rusage));
    inicializar_stats(&p1_full_stats);
    inicializar_stats(&p2_full_stats);
    inicializar_stats(&p3_full_stats);
}

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

void proceso_terminado(const char *nombre_proceso, pid_t pid)
{
    printf("\n" COLOR_KERNEL "[Control Central] " ANSI_RESET "%s%s (PID %d) ha completado su tarea." ANSI_RESET "\n",
           color_proceso(nombre_proceso), nombre_legible(nombre_proceso), pid);
    fflush(stdout);
}

void print_fila_tabla(const char *nombre, pid_t pid, double wall_time, struct rusage *usage)
{
    printf("%s| %-20s | %-7d | %-11.6f | %ld.%06ld s | %ld.%06ld s | %-15ld |" ANSI_RESET "\n",
           color_proceso(nombre),
           nombre_legible(nombre), pid,
           wall_time,
           usage->ru_utime.tv_sec, usage->ru_utime.tv_usec,
           usage->ru_stime.tv_sec, usage->ru_stime.tv_usec,
           usage->ru_maxrss);
}

void mostrar_tabla_recursos()
{
    printf(COLOR_TABLE "\n--- Resumen de Recursos del Ciclo ---\n");
    printf("| Proceso              | PID     | T. Real (s) | CPU Usuario | CPU Sistema | Memoria Pico (KB) |\n");
    printf("|----------------------|---------|-------------|-------------|-------------|-------------------|\n" ANSI_RESET);

    if (pid_p1 > 0)
        print_fila_tabla("./proceso1", pid_p1, time_p1, &usage_p1);
    if (pid_p2 > 0)
        print_fila_tabla("./proceso2", pid_p2, time_p2, &usage_p2);
    if (pid_p3 > 0)
        print_fila_tabla("./proceso3", pid_p3, time_p3, &usage_p3);

    printf(COLOR_TABLE "|----------------------|---------|-------------|-------------|-------------|-------------------|\n" ANSI_RESET);
    fflush(stdout);
}

void mostrar_metricas_extra(ProcesoStats *stats)
{
    if (stats->time_real == 0.0)
        return;

    printf(COLOR_TABLE "\n  [METRICAS AVANZADAS]\n");
    printf("  - T. Ejecución Efectiva (CPU): %.6f s\n", stats->tiempo_ejecucion_efectiva);
    printf("  - Cambios de Contexto (Vol/Inv): %d / %d\n", stats->cambios_contexto_voluntario, stats->cambios_contexto_involuntario);

    if (stats->num_pausas > 0)
    {
        printf("  - Pausas Totales: %d\n", stats->num_pausas);
        printf("  - T. Pausado Total (Wall): %.6f s\n", stats->tiempo_pausado_total);
        printf("  - Quantum Dado/Usado: %.2f s / %.2f s\n", stats->quantum_dado_total, stats->quantum_usado_total);
    }

    const char *estado;
    if (WIFEXITED(stats->exit_status))
        estado = "TERMINADO_EXIT";
    else if (WIFSIGNALED(stats->exit_status))
        estado = "TERMINADO_SENAL";
    else
        estado = "NO_TERMINADO";

    printf("  - Estado Final: %s (Status: %d)\n" ANSI_RESET, estado, stats->exit_status);
}

void esperar_proceso(pid_t pid, const char *nombre_proceso, int timeout_sec, struct timeval *start_time)
{
    int status;
    struct rusage usage;
    struct timeval end_time;
    double wall_time = 0.0;
    int kill_signal = 0;

    if (timeout_sec > 0)
    {
        for (int remaining = timeout_sec; remaining > 0; remaining--)
        {
            pid_t terminado = wait4(pid, &status, WNOHANG, &usage);
            if (terminado == pid)
            {
                gettimeofday(&end_time, NULL);
                wall_time = timeval_diff(start_time, &end_time);
                proceso_terminado(nombre_proceso, pid);
                goto guardar_stats;
            }
            sleep(1);
        }

        printf(COLOR_ERROR "[Control Central] ALERTA: " ANSI_RESET "%s%s (PID %d) no responde. Forzando terminación (SIGKILL)..." ANSI_RESET "\n",
               color_proceso(nombre_proceso), nombre_legible(nombre_proceso), pid);
        kill(pid, SIGKILL);
        kill_signal = SIGKILL;
        wait4(pid, &status, 0, &usage);
        gettimeofday(&end_time, NULL);
        wall_time = timeval_diff(start_time, &end_time);
        proceso_terminado(nombre_proceso, pid);
    }
    else
    {
        printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Esperando finalización de %s%s (PID %d)..." ANSI_RESET "\n",
               color_proceso(nombre_proceso), nombre_legible(nombre_proceso), pid);
        fflush(stdout);
        wait4(pid, &status, 0, &usage);
        gettimeofday(&end_time, NULL);
        wall_time = timeval_diff(start_time, &end_time);
        proceso_terminado(nombre_proceso, pid);
    }

guardar_stats:
    guardar_stats_proceso(nombre_proceso, pid, wall_time, &usage, status);

    if (strstr(nombre_proceso, "proceso1"))
        p1_full_stats.seniales_recibidas[kill_signal]++;
    if (strstr(nombre_proceso, "proceso2"))
        p2_full_stats.seniales_recibidas[kill_signal]++;
    if (strstr(nombre_proceso, "proceso3"))
        p3_full_stats.seniales_recibidas[kill_signal]++;

    if (escenario_actual == 1)
    {
        printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Mostrando métricas adicionales para %s%s:\n",
               color_proceso(nombre_proceso), nombre_legible(nombre_proceso));
        if (strstr(nombre_proceso, "proceso1"))
            mostrar_metricas_extra(&p1_full_stats);
        if (strstr(nombre_proceso, "proceso2"))
            mostrar_metricas_extra(&p2_full_stats);
        if (strstr(nombre_proceso, "proceso3"))
            mostrar_metricas_extra(&p3_full_stats);
    }
}

void lanzar_hijo_exec(char *const argv[])
{
    execvp("qemu-riscv32", argv);
    perror(COLOR_ERROR "Error al iniciar componente de software" ANSI_RESET);
    exit(1);
}

int leer_datos_p3(int pipe_fd)
{
    char buffer[128];

    ssize_t bytes = read(pipe_fd, buffer, sizeof(buffer) - 1);
    int valor = 0;

    if (bytes > 0)
    {
        buffer[bytes] = '\0';

        for (int i = 0; i < bytes; i++)
        {
            if (buffer[i] == '\n')
                buffer[i] = ' ';
        }

        if (bytes > 0 && buffer[bytes - 1] == ' ')
            buffer[bytes - 1] = '\0';

        printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Análisis recibido de %s%s" COLOR_KERNEL ": [%s]" ANSI_RESET "\n",
               color_proceso("./proceso3"), nombre_legible("./proceso3"), buffer);

        valor = atoi(buffer);
    }

    return valor;
}

void enviar_contenido_archivo_a_pipe(int pipe_fd_escritura, const char *archivo)
{
    FILE *fp = fopen(archivo, "r");
    if (!fp)
    {
        fprintf(stderr, COLOR_ERROR "[Control Central] ERROR: No se puede abrir el archivo de señal %s\n" ANSI_RESET, archivo);
        return;
    }

    char datos_leidos;
    while ((datos_leidos = fgetc(fp)) != EOF)
        write(pipe_fd_escritura, &datos_leidos, 1);

    fclose(fp);
}

unsigned long obtener_pc_riscv(const char *ruta_log)
{
    char comando[256];

    snprintf(comando, sizeof(comando), "tail -n 200 %s", ruta_log);

    FILE *fp = popen(comando, "r");
    if (!fp)
        return 0;

    char linea[256];
    unsigned long ultimo_pc = 0;

    while (fgets(linea, sizeof(linea), fp))
    {

        for (int i = 0; linea[i]; i++)
            linea[i] = tolower(linea[i]);

        char *ptr = linea;
        while ((ptr = strstr(ptr, "pc")) != NULL)
        {

            int inicio_valido = (ptr == linea) || !isalnum(*(ptr - 1));

            int fin_valido = !isalnum(*(ptr + 2));

            if (inicio_valido && fin_valido)
            {

                char *num_ptr = ptr + 2;

                while (*num_ptr && !isxdigit(*num_ptr))
                    num_ptr++;

                if (*num_ptr)
                {
                    ultimo_pc = strtoul(num_ptr, NULL, 16);
                }
            }

            ptr += 2;
        }
    }

    pclose(fp);
    return ultimo_pc;
}

void ejecutar_escenario_1()
{
    pid_t pid1, pid2, pid3;
    const int TIMEOUT_P3 = 10;

    int datos_pipe_p3[2];
    int p1_input_pipe[2];
    int p1_to_p3_pipe[2];

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

    gettimeofday(&p1_start, NULL);

    if ((pid1 = fork()) == 0)
    {
        close(p1_input_pipe[1]);
        dup2(p1_input_pipe[0], STDIN_FILENO);
        close(p1_input_pipe[0]);

        close(p1_to_p3_pipe[0]);
        dup2(p1_to_p3_pipe[1], STDOUT_FILENO);
        close(p1_to_p3_pipe[1]);

        close(datos_pipe_p3[0]);
        close(datos_pipe_p3[1]);

        char *argv[] = {"qemu-riscv32", "./code/escenariosBasicos/proceso1", NULL};
        lanzar_hijo_exec(argv);
    }

    close(p1_input_pipe[0]);
    close(p1_to_p3_pipe[1]);

    enviar_contenido_archivo_a_pipe(p1_input_pipe[1], "medidas.txt");
    close(p1_input_pipe[1]);

    gettimeofday(&p2_start, NULL);

    if ((pid2 = fork()) == 0)
    {
        close(datos_pipe_p3[0]);
        close(datos_pipe_p3[1]);
        close(p1_input_pipe[0]);
        close(p1_input_pipe[1]);
        close(p1_to_p3_pipe[0]);
        close(p1_to_p3_pipe[1]);

        char *argv[] = {"qemu-riscv32", "./code/escenariosBasicos/proceso2", NULL};
        lanzar_hijo_exec(argv);
    }

    gettimeofday(&p3_start, NULL);

    if ((pid3 = fork()) == 0)
    {
        close(datos_pipe_p3[0]);
        dup2(datos_pipe_p3[1], STDOUT_FILENO);
        close(datos_pipe_p3[1]);

        close(p1_to_p3_pipe[1]);
        dup2(p1_to_p3_pipe[0], STDIN_FILENO);
        close(p1_to_p3_pipe[0]);

        close(p1_input_pipe[0]);
        close(p1_input_pipe[1]);

        char *argv[] = {"qemu-riscv32", "./code/escenariosBasicos/proceso3", NULL};
        lanzar_hijo_exec(argv);
    }

    close(datos_pipe_p3[1]);
    close(p1_to_p3_pipe[0]);

    kill(pid2, SIGSTOP);
    p2_full_stats.seniales_recibidas[SIGSTOP]++;
    p2_full_stats.num_pausas++;
    kill(pid3, SIGSTOP);
    p3_full_stats.seniales_recibidas[SIGSTOP]++;
    p3_full_stats.num_pausas++;

    esperar_proceso(pid1, "./code/escenariosBasicos/proceso1", 0, &p1_start);
    close(p1_to_p3_pipe[0]);

    struct timeval p1_end, p2_activacion;
    gettimeofday(&p1_end, NULL);
    p2_full_stats.tiempo_pausado_total += timeval_diff(&p2_start, &p1_end);

    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Receptor (P1) terminó. Activando %s%s" COLOR_KERNEL "..." ANSI_RESET "\n", color_proceso("proceso2"), nombre_legible("proceso2"));

    gettimeofday(&p2_activacion, NULL);
    kill(pid2, SIGCONT);
    p2_full_stats.seniales_recibidas[SIGCONT]++;
    esperar_proceso(pid2, "./code/escenariosBasicos/proceso2", 0, &p2_activacion);

    struct timeval p2_end, p3_activacion;
    gettimeofday(&p2_end, NULL);
    p3_full_stats.tiempo_pausado_total += timeval_diff(&p3_start, &p2_end);

    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Escudo (P2) terminó. Activando %s%s" COLOR_KERNEL "..." ANSI_RESET "\n", color_proceso("proceso3"), nombre_legible("proceso3"));

    gettimeofday(&p3_activacion, NULL);
    kill(pid3, SIGCONT);
    p3_full_stats.seniales_recibidas[SIGCONT]++;
    esperar_proceso(pid3, "./code/escenariosBasicos/proceso3", 0, &p3_activacion);

    leer_datos_p3(datos_pipe_p3[0]);
}

void ejecutar_escenario_2()
{
    pid_t pid1, pid2, pid3;
    const int QUANTUM_P1 = 10;
    const int QUANTUM_P3 = 5;

    int p1_input_pipe[2];
    int p1_to_p3_pipe[2];
    int p3_to_kernel_pipe[2];

    struct rusage usage_temp;
    int p1_status, p3_status;
    int p1_vivo = 1, p3_vivo = 1;
    int last_temp = -1;
    double tiempo_acumulado_p1 = 0.0;
    double tiempo_acumulado_p3 = 0.0;
    struct timeval turno_start, turno_end;
    struct timeval p1_last_stop_time, p3_last_stop_time;
    struct timeval p1_turn_start, p3_turn_start;

    if (pipe(p1_input_pipe) == -1 || pipe(p1_to_p3_pipe) == -1 || pipe(p3_to_kernel_pipe) == -1)
    {
        perror("Error en pipes");
        exit(1);
    }

    if ((pid1 = fork()) == 0)
    {
        close(p1_input_pipe[1]);
        dup2(p1_input_pipe[0], STDIN_FILENO);
        close(p1_input_pipe[0]);

        close(p1_to_p3_pipe[0]);
        dup2(p1_to_p3_pipe[1], STDOUT_FILENO);
        close(p1_to_p3_pipe[1]);

        close(p3_to_kernel_pipe[0]);
        close(p3_to_kernel_pipe[1]);

        char *argv[] = {
            "qemu-riscv32",
            "-d", "cpu",
            "-D", "p1_trace.log",
            "./code/escenariosBasicos/proceso1",
            NULL};
        lanzar_hijo_exec(argv);
    }

    if ((pid3 = fork()) == 0)
    {
        close(p1_input_pipe[0]);
        close(p1_input_pipe[1]);

        close(p1_to_p3_pipe[1]);
        dup2(p1_to_p3_pipe[0], STDIN_FILENO);
        close(p1_to_p3_pipe[0]);

        close(p3_to_kernel_pipe[0]);
        dup2(p3_to_kernel_pipe[1], STDOUT_FILENO);
        close(p3_to_kernel_pipe[1]);

        char *argv[] = {
            "qemu-riscv32",
            "-d", "cpu",
            "-D", "p3_trace.log",
            "./code/escenariosBasicos/proceso3",
            NULL};
        lanzar_hijo_exec(argv);
    }

    close(p1_input_pipe[0]);
    close(p1_to_p3_pipe[0]);
    close(p1_to_p3_pipe[1]);
    close(p3_to_kernel_pipe[1]);

    fcntl(p3_to_kernel_pipe[0], F_SETFL, O_NONBLOCK);

    kill(pid1, SIGSTOP);
    p1_full_stats.seniales_recibidas[SIGSTOP]++;
    gettimeofday(&p1_last_stop_time, NULL);
    kill(pid3, SIGSTOP);
    p3_full_stats.seniales_recibidas[SIGSTOP]++;
    gettimeofday(&p3_last_stop_time, NULL);

    p1_full_stats.num_pausas++;
    p3_full_stats.num_pausas++;

    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Procesos listos. Iniciando...\n");

    enviar_contenido_archivo_a_pipe(p1_input_pipe[1], "medidas.txt");
    close(p1_input_pipe[1]);

    while (p1_vivo || p3_vivo)
    {

        if (p1_vivo)
        {
            printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Activando %s%s (PID %d) por %d seg...\n",
                   color_proceso("./proceso1"), nombre_legible("./proceso1"), pid1, QUANTUM_P1);

            struct timeval now;
            gettimeofday(&now, NULL);
            p1_full_stats.tiempo_pausado_total += timeval_diff(&p1_last_stop_time, &now);

            gettimeofday(&turno_start, NULL);
            p1_turn_start = turno_start;
            kill(pid1, SIGCONT);
            p1_full_stats.seniales_recibidas[SIGCONT]++;
            sleep(QUANTUM_P1);
            gettimeofday(&turno_end, NULL);
            tiempo_acumulado_p1 += timeval_diff(&turno_start, &turno_end);
            p1_full_stats.quantum_dado_total += QUANTUM_P1;

            if (wait4(pid1, &p1_status, WNOHANG, &usage_temp) == pid1)
            {
                p1_vivo = 0;
                p1_full_stats.quantum_usado_total += timeval_diff(&p1_turn_start, &turno_end);
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "%s%s (PID %d) terminó." ANSI_RESET "\n",
                       color_proceso("./proceso1"), nombre_legible("./proceso1"), pid1);
                guardar_stats_proceso("./code/escenariosBasicos/proceso1", pid1, tiempo_acumulado_p1, &usage_temp, p1_status);
            }
            else
            {
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Deteniendo %s%s (PID %d). Tiempo agotado.\n",
                       color_proceso("./proceso1"), nombre_legible("./proceso1"), pid1);
                kill(pid1, SIGSTOP);
                usleep(10000);
                unsigned long pc_p1 = obtener_pc_riscv("p1_trace.log");
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "El proceso 1 se quedó en el PC: 0x%lx\n" ANSI_RESET, pc_p1);
                p1_full_stats.seniales_recibidas[SIGSTOP]++;
                gettimeofday(&p1_last_stop_time, NULL);
                p1_full_stats.num_pausas++;
                p1_full_stats.quantum_usado_total += timeval_diff(&p1_turn_start, &p1_last_stop_time);
            }
        }

        if (p3_vivo)
        {
            printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Activando %s%s (PID %d) por %d seg...\n",
                   color_proceso("./proceso3"), nombre_legible("./proceso3"), pid3, QUANTUM_P3);

            struct timeval now;
            gettimeofday(&now, NULL);
            p3_full_stats.tiempo_pausado_total += timeval_diff(&p3_last_stop_time, &now);

            gettimeofday(&turno_start, NULL);
            p3_turn_start = turno_start;
            kill(pid3, SIGCONT);
            p3_full_stats.seniales_recibidas[SIGCONT]++;
            sleep(QUANTUM_P3);

            last_temp = leer_datos_p3(p3_to_kernel_pipe[0]);

            printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Temperatura analizada por %s%s: %d\n",
                   color_proceso("./proceso3"), nombre_legible("./proceso3"), last_temp);

            gettimeofday(&turno_end, NULL);
            tiempo_acumulado_p3 += timeval_diff(&turno_start, &turno_end);
            p3_full_stats.quantum_dado_total += QUANTUM_P3;

            if (wait4(pid3, &p3_status, WNOHANG, &usage_temp) == pid3)
            {
                p3_vivo = 0;
                p3_full_stats.quantum_usado_total += timeval_diff(&p3_turn_start, &turno_end);
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "%s%s (PID %d) terminó." ANSI_RESET "\n",
                       color_proceso("./proceso3"), nombre_legible("./proceso3"), pid3);
                guardar_stats_proceso("./code/escenariosBasicos/proceso3", pid3, tiempo_acumulado_p3, &usage_temp, p3_status);
            }
            else
            {
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Deteniendo %s%s (PID %d). Tiempo agotado.\n",
                       color_proceso("./proceso3"), nombre_legible("./proceso3"), pid3);
                kill(pid3, SIGSTOP);
                usleep(10000);
                unsigned long pc_p1 = obtener_pc_riscv("p3_trace.log");
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "El proceso 1 se quedó en el PC: 0x%lx\n" ANSI_RESET, pc_p1);
                p3_full_stats.seniales_recibidas[SIGSTOP]++;
                gettimeofday(&p3_last_stop_time, NULL);
                p3_full_stats.num_pausas++;
                p3_full_stats.quantum_usado_total += timeval_diff(&p3_turn_start, &p3_last_stop_time);
            }
        }

        if (last_temp != -1)
        {
            printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Lanzando %s%s con argumento: %d (Acción por defecto si < 90).\n",
                   color_proceso("./proceso2"), nombre_legible("./proceso2"), (last_temp > 90) ? 1 : 0);

            gettimeofday(&p2_start, NULL);

            if ((pid2 = fork()) == 0)
            {
                close(p3_to_kernel_pipe[0]);
                char temp_arg[2] = {(last_temp > 90) ? '1' : '0', '\0'};
                char *args[] = {"qemu-riscv32", "./code/escenariosBasicos/proceso2", temp_arg, NULL};
                lanzar_hijo_exec(args);
            }

            esperar_proceso(pid2, "./code/escenariosBasicos/proceso2", 0, &p2_start);
        }
    }

    close(p3_to_kernel_pipe[0]);

    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Ciclo de Round-Robin finalizado.\n");
}

void ejecutar_escenario_3()
{
    pid_t pid1, pid2, pid3;
    const int TIMEOUT_P1 = 10;
    const int TIMEOUT_P3 = 5;

    int p1_input_pipe[2], p1_to_p3_pipe[2], p3_to_kernel_pipe[2];

    struct rusage usage_temp;
    int p1_status, p3_status;
    int p1_vivo = 1, p3_vivo = 1;
    int arg_p2_proximo = -1;

    double tiempo_acumulado_p1 = 0.0;
    double tiempo_acumulado_p3 = 0.0;
    struct timeval turno_start, turno_end;
    struct timeval p1_last_stop_time, p3_last_stop_time;
    struct timeval p1_turn_start, p3_turn_start;

    if (pipe(p1_input_pipe) == -1 || pipe(p1_to_p3_pipe) == -1 || pipe(p3_to_kernel_pipe) == -1)
    {
        perror("pipe");
        exit(1);
    }

    if ((pid1 = fork()) == 0)
    {
        close(p1_input_pipe[1]);
        dup2(p1_input_pipe[0], STDIN_FILENO);
        close(p1_input_pipe[0]);

        close(p1_to_p3_pipe[0]);
        dup2(p1_to_p3_pipe[1], STDOUT_FILENO);
        close(p1_to_p3_pipe[1]);

        close(p3_to_kernel_pipe[0]);
        close(p3_to_kernel_pipe[1]);

        char *argv[] = {"qemu-riscv32", "./code/escenariosBasicos/proceso1", NULL};
        lanzar_hijo_exec(argv);
    }

    if ((pid3 = fork()) == 0)
    {
        close(p1_input_pipe[0]);
        close(p1_input_pipe[1]);

        close(p1_to_p3_pipe[1]);
        dup2(p1_to_p3_pipe[0], STDIN_FILENO);
        close(p1_to_p3_pipe[0]);

        close(p3_to_kernel_pipe[0]);
        dup2(p3_to_kernel_pipe[1], STDOUT_FILENO);
        close(p3_to_kernel_pipe[1]);

        char *argv[] = {"qemu-riscv32", "./code/escenariosBasicos/proceso3", NULL};
        lanzar_hijo_exec(argv);
    }

    close(p1_input_pipe[0]);
    close(p1_to_p3_pipe[0]);
    close(p1_to_p3_pipe[1]);
    close(p3_to_kernel_pipe[1]);

    fcntl(p3_to_kernel_pipe[0], F_SETFL, O_NONBLOCK);

    kill(pid1, SIGSTOP);
    p1_full_stats.seniales_recibidas[SIGSTOP]++;
    gettimeofday(&p1_last_stop_time, NULL);
    kill(pid3, SIGSTOP);
    p3_full_stats.seniales_recibidas[SIGSTOP]++;
    gettimeofday(&p3_last_stop_time, NULL);

    p1_full_stats.num_pausas++;
    p3_full_stats.num_pausas++;

    enviar_contenido_archivo_a_pipe(p1_input_pipe[1], "medidas.txt");
    close(p1_input_pipe[1]);

    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Procesos listos. Iniciando...\n");

    while (p1_vivo || p3_vivo)
    {

        {
            char *arg_msg = (arg_p2_proximo == 1) ? "Activar (1)" : (arg_p2_proximo == 0) ? "Desactivar (0)"
                                                                                          : "Neutro (-1)";
            printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Lanzando %s%s con argumento: %s (Decisión de ronda previa)...\n",
                   color_proceso("proceso2"), nombre_legible("proceso2"), arg_msg);

            gettimeofday(&p2_start, NULL);

            if ((pid2 = fork()) == 0)
            {
                close(p3_to_kernel_pipe[0]);

                if (arg_p2_proximo == -1)
                {
                    char *argv[] = {"qemu-riscv32", "./code/escenariosBasicos/proceso2", NULL};
                    lanzar_hijo_exec(argv);
                }
                else
                {
                    char arg_str[2];
                    sprintf(arg_str, "%d", arg_p2_proximo);
                    char *argv[] = {"qemu-riscv32", "./code/escenariosBasicos/proceso2", arg_str, NULL};
                    lanzar_hijo_exec(argv);
                }
            }

            esperar_proceso(pid2, "proceso2", 0, &p2_start);

            arg_p2_proximo = -1;
        }

        if (p1_vivo)
        {
            printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Activando %s%s (PID %d) por %ds...\n",
                   color_proceso("./proceso1"), nombre_legible("./proceso1"), pid1, TIMEOUT_P1);

            struct timeval now;
            gettimeofday(&now, NULL);
            p1_full_stats.tiempo_pausado_total += timeval_diff(&p1_last_stop_time, &now);

            gettimeofday(&turno_start, NULL);
            p1_turn_start = turno_start;
            kill(pid1, SIGCONT);
            p1_full_stats.seniales_recibidas[SIGCONT]++;
            sleep(TIMEOUT_P1);

            gettimeofday(&turno_end, NULL);
            tiempo_acumulado_p1 += timeval_diff(&turno_start, &turno_end);
            p1_full_stats.quantum_dado_total += TIMEOUT_P1;

            if (wait4(pid1, &p1_status, WNOHANG, &usage_temp) == pid1)
            {
                p1_vivo = 0;
                p1_full_stats.quantum_usado_total += timeval_diff(&p1_turn_start, &turno_end);
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "%s%s (PID %d) terminó.\n",
                       color_proceso("./proceso1"), nombre_legible("./proceso1"), pid1);
                guardar_stats_proceso("./code/escenariosBasicos/proceso1", pid1, tiempo_acumulado_p1, &usage_temp, p1_status);
            }
            else
            {
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Deteniendo %s%s (PID %d). Tiempo agotado.\n",
                       color_proceso("./proceso1"), nombre_legible("./proceso1"), pid1);
                kill(pid1, SIGSTOP);
                p1_full_stats.seniales_recibidas[SIGSTOP]++;
                gettimeofday(&p1_last_stop_time, NULL);
                p1_full_stats.num_pausas++;
                p1_full_stats.quantum_usado_total += timeval_diff(&p1_turn_start, &p1_last_stop_time);
            }
        }

        if (p3_vivo)
        {
            printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Activando %s%s (PID %d) por %ds...\n",
                   color_proceso("./proceso3"), nombre_legible("./proceso3"), pid3, TIMEOUT_P3);

            struct timeval now;
            gettimeofday(&now, NULL);
            p3_full_stats.tiempo_pausado_total += timeval_diff(&p3_last_stop_time, &now);

            gettimeofday(&turno_start, NULL);
            p3_turn_start = turno_start;
            kill(pid3, SIGCONT);
            p3_full_stats.seniales_recibidas[SIGCONT]++;
            sleep(TIMEOUT_P3);

            int ultimo_valor_del_turno = leer_datos_p3(p3_to_kernel_pipe[0]);

            if (ultimo_valor_del_turno != 0)
            {
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Última lectura procesada por %s%s: %d\n",
                       color_proceso("./proceso3"), nombre_legible("./proceso3"), ultimo_valor_del_turno);

                if (ultimo_valor_del_turno > 90)
                {
                    arg_p2_proximo = 1;
                    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Activar escudo para la próxima ronda.\n" ANSI_RESET);
                }
                else
                {
                    arg_p2_proximo = 0;
                    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Desactivar escudo para la próxima ronda.\n" ANSI_RESET);
                }
            }
            else
            {
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Mantener estado neutro para la próxima ronda (No se recibió un nuevo valor del analizador).\n" ANSI_RESET);
            }

            gettimeofday(&turno_end, NULL);
            tiempo_acumulado_p3 += timeval_diff(&turno_start, &turno_end);
            p3_full_stats.quantum_dado_total += TIMEOUT_P3;

            if (wait4(pid3, &p3_status, WNOHANG, &usage_temp) == pid3)
            {
                p3_vivo = 0;
                p3_full_stats.quantum_usado_total += timeval_diff(&p3_turn_start, &turno_end);
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "%s%s (PID %d) terminó.\n",
                       color_proceso("./proceso3"), nombre_legible("./proceso3"), pid3);
                guardar_stats_proceso("./code/escenariosBasicos/proceso3", pid3, tiempo_acumulado_p3, &usage_temp, p3_status);

                if (p1_vivo)
                {
                    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Terminó %s%s. Forzando terminación de %s%s (PID %d)...\n",
                           color_proceso("./proceso3"), nombre_legible("./proceso3"),
                           color_proceso("./proceso1"), nombre_legible("./proceso1"), pid1);
                    kill(pid1, SIGKILL);
                    p1_full_stats.seniales_recibidas[SIGKILL]++;
                    wait4(pid1, &p1_status, 0, &usage_temp);

                    guardar_stats_proceso("./code/escenariosBasicos/proceso1", pid1, tiempo_acumulado_p1, &usage_temp, p1_status);
                    p1_vivo = 0;
                }
            }
            else
            {
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Deteniendo %s%s (PID %d). Tiempo agotado.\n",
                       color_proceso("./proceso3"), nombre_legible("./proceso3"), pid3);
                kill(pid3, SIGSTOP);
                p3_full_stats.seniales_recibidas[SIGSTOP]++;
                gettimeofday(&p3_last_stop_time, NULL);
                p3_full_stats.num_pausas++;
                p3_full_stats.quantum_usado_total += timeval_diff(&p3_turn_start, &p3_last_stop_time);
            }
        }
    }

    close(p3_to_kernel_pipe[0]);

    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Ciclo finalizado.\n");
}

void ejecutar_escenario()
{
    switch (escenario_actual)
    {
    case 1:
        printf(COLOR_CICLO "--- Escenario 1: [Receptor] -> [Escudo] -> [Analizador] ---\n" ANSI_RESET);
        ejecutar_escenario_1();
        break;
    case 2:
        printf(COLOR_CICLO "--- Escenario 2: [Receptor] -> [Analizador] -> [Escudo] ---\n" ANSI_RESET);
        ejecutar_escenario_2();
        break;
    case 3:
        printf(COLOR_CICLO "--- Escenario 3: [Escudo] -> [Receptor] -> [Analizador] ---\n" ANSI_RESET);
        ejecutar_escenario_3();
        break;
    case 4:
        printf(COLOR_CICLO "--- Escenario 4: En desarrollo ---\n" ANSI_RESET);
        break;
    default:
        printf(COLOR_ERROR "[Control Central] Error: Escenario de ejecución no válido." ANSI_RESET "\n");
        break;
    }
}

void reiniciar_escenario()
{
    system("clear");
    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Reinicio de escenario solicitado (SIGTSTP). Seleccione nuevo escenario." ANSI_RESET "\n");
    escenario_actual = 0;
    ciclo_actual = 1;

    memset(&acumulador_global, 0, sizeof(AcumuladorMetricas));
    indice_resultados = 0;
}

void acumular_metricas_ciclo(double tiempo_total_ciclo)
{
    acumulador_global.total_ciclos_acumulados++;
    acumulador_global.tiempo_real_total += tiempo_total_ciclo;

    acumulador_global.cpu_usuario_total += p1_full_stats.ru_utime_sec + (double)p1_full_stats.ru_utime_usec / 1000000.0;
    acumulador_global.cpu_usuario_total += p2_full_stats.ru_utime_sec + (double)p2_full_stats.ru_utime_usec / 1000000.0;
    acumulador_global.cpu_usuario_total += p3_full_stats.ru_utime_sec + (double)p3_full_stats.ru_utime_usec / 1000000.0;

    acumulador_global.cpu_sistema_total += p1_full_stats.ru_stime_sec + (double)p1_full_stats.ru_stime_usec / 1000000.0;
    acumulador_global.cpu_sistema_total += p2_full_stats.ru_stime_sec + (double)p2_full_stats.ru_stime_usec / 1000000.0;
    acumulador_global.cpu_sistema_total += p3_full_stats.ru_stime_sec + (double)p3_full_stats.ru_stime_usec / 1000000.0;

    acumulador_global.memoria_pico_total_kb += p1_full_stats.ru_maxrss;
    acumulador_global.memoria_pico_total_kb += p2_full_stats.ru_maxrss;
    acumulador_global.memoria_pico_total_kb += p3_full_stats.ru_maxrss;

    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Métricas del ciclo #%d acumuladas. (%d/%d para reporte global)\n" ANSI_RESET,
           ciclo_actual - 1, acumulador_global.total_ciclos_acumulados, CICLOS_POR_REPORTE);
}

void imprimir_reporte_acumulado()
{
    double cpu_total = acumulador_global.cpu_usuario_total + acumulador_global.cpu_sistema_total;

    printf(COLOR_ACUMULADO "\n======================================================\n");
    printf("== REPORTE GLOBAL ACUMULADO (%d CICLOS) ==\n", acumulador_global.total_ciclos_acumulados);
    printf("======================================================\n" ANSI_RESET);

    printf(COLOR_TABLE "• Tiempo Real Total (Wall Time): %.6f s\n", acumulador_global.tiempo_real_total);
    printf("• Tiempo Total de CPU (Usuario + Sistema): %.6f s\n", cpu_total);
    printf("  - CPU Usuario Acumulado: %.6f s\n", acumulador_global.cpu_usuario_total);
    printf("  - CPU Sistema Acumulado: %.6f s\n", acumulador_global.cpu_sistema_total);
    printf("• Memoria Total (Suma de Picos): %ld KB\n", acumulador_global.memoria_pico_total_kb);
    printf(COLOR_ACUMULADO "======================================================\n" ANSI_RESET);

    memset(&acumulador_global, 0, sizeof(AcumuladorMetricas));
}

void imprimir_metricas_rr()
{
    printf(COLOR_TABLE "\n--- Métricas Avanzadas de Ejecución (E2/E3) ---\n");

    if (pid_p1 > 0)
    {
        printf(COLOR_P1 "Proceso: %s (PID %d)\n", nombre_legible("./proceso1"), pid_p1);
        mostrar_metricas_extra(&p1_full_stats);
    }
    if (pid_p2 > 0)
    {
        printf(COLOR_P2 "\nProceso: %s (PID %d)\n", nombre_legible("./proceso2"), pid_p2);
        mostrar_metricas_extra(&p2_full_stats);
    }
    if (pid_p3 > 0)
    {
        printf(COLOR_P3 "\nProceso: %s (PID %d)\n", nombre_legible("./proceso3"), pid_p3);
        mostrar_metricas_extra(&p3_full_stats);
    }

    printf(COLOR_TABLE "--------------------------------------------------\n" ANSI_RESET);
    fflush(stdout);
}

void escribir_json_stats(FILE *fp, const char *nombre, ProcesoStats *stats, int primer_proceso)
{
    if (!primer_proceso)
        fprintf(fp, ",\n");
    fprintf(fp, "\t\t\"%s\": {\n", nombre);
    fprintf(fp, "\t\t\t\"time_real\": %.6f,\n", stats->time_real);
    fprintf(fp, "\t\t\t\"cpu_user\": %ld.%06ld,\n", stats->ru_utime_sec, stats->ru_utime_usec);
    fprintf(fp, "\t\t\t\"cpu_sys\": %ld.%06ld,\n", stats->ru_stime_sec, stats->ru_stime_usec);
    fprintf(fp, "\t\t\t\"memoria_pico_kb\": %ld,\n", stats->ru_maxrss);
    fprintf(fp, "\t\t\t\"cambios_contexto_vol\": %d,\n", stats->cambios_contexto_voluntario);
    fprintf(fp, "\t\t\t\"cambios_contexto_inv\": %d,\n", stats->cambios_contexto_involuntario);
    fprintf(fp, "\t\t\t\"ejecucion_efectiva\": %.6f", stats->tiempo_ejecucion_efectiva);

    if (stats->num_pausas > 0)
    {
        fprintf(fp, ",\n");
        fprintf(fp, "\t\t\t\"num_pausas\": %d,\n", stats->num_pausas);
        fprintf(fp, "\t\t\t\"tiempo_pausado_total\": %.6f,\n", stats->tiempo_pausado_total);
        fprintf(fp, "\t\t\t\"quantum_dado\": %.6f,\n", stats->quantum_dado_total);
        fprintf(fp, "\t\t\t\"quantum_usado\": %.6f\n", stats->quantum_usado_total);
    }
    else
    {
        fprintf(fp, "\n");
    }

    fprintf(fp, "\t\t}");
}

void exportar_reporte_acumulado_a_json()
{

    char nombre_archivo[64];
    snprintf(nombre_archivo, sizeof(nombre_archivo), "metricas_total_%d.json", escenario_actual);

    FILE *fp = fopen(nombre_archivo, "w");

    if (fp == NULL)
    {
        fprintf(stderr, COLOR_ERROR "Error al abrir %s\n" ANSI_RESET, nombre_archivo);
        return;
    }

    double cpu_total = acumulador_global.cpu_usuario_total + acumulador_global.cpu_sistema_total;

    fprintf(fp, "{\n");
    fprintf(fp, "\t\"escenario\": %d,\n", escenario_actual);
    fprintf(fp, "\t\"total_ciclos_acumulados\": %d,\n", acumulador_global.total_ciclos_acumulados);
    fprintf(fp, "\t\"tiempo_real_total\": %.6f,\n", acumulador_global.tiempo_real_total);
    fprintf(fp, "\t\"tiempo_total_cpu\": %.6f,\n", cpu_total);
    fprintf(fp, "\t\"cpu_usuario_acumulado\": %.6f,\n", acumulador_global.cpu_usuario_total);
    fprintf(fp, "\t\"cpu_sistema_acumulado\": %.6f,\n", acumulador_global.cpu_sistema_total);
    fprintf(fp, "\t\"memoria_pico_total_kb\": %ld\n", acumulador_global.memoria_pico_total_kb);
    fprintf(fp, "}\n");

    fclose(fp);

    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "REPORTE GLOBAL JSON: Exportado a '%s'.\n" ANSI_RESET, nombre_archivo);
}

void exportar_resultados_a_json()
{
    char nombre_archivo[64];
    snprintf(nombre_archivo, sizeof(nombre_archivo), "metricas_mision_%d.json", escenario_actual);
    FILE *fp = fopen(nombre_archivo, "a");
    if (fp == NULL)
    {
        perror(COLOR_ERROR "Error al abrir metricas_mision.json" ANSI_RESET);
        return;
    }

    if (ftell(fp) == 0)
    {
        fprintf(fp, "[\n");
    }
    else
    {
        fseek(fp, -2, SEEK_END);
        char last_char = fgetc(fp);
        if (last_char != '[')
        {
            fprintf(fp, ",\n");
        }
        else
        {
            fseek(fp, 0, SEEK_END);
        }
    }

    for (int i = 0; i < indice_resultados; i++)
    {
        if (i > 0)
            fprintf(fp, ",\n");

        fprintf(fp, "\t{\n");
        fprintf(fp, "\t\t\"ciclo\": %d,\n", resultados_ciclos[i].ciclo);
        fprintf(fp, "\t\t\"escenario\": %d,\n", resultados_ciclos[i].escenario);
        fprintf(fp, "\t\t\"tiempo_total_ciclo\": %.6f,\n", resultados_ciclos[i].tiempo_total_ciclo);
        fprintf(fp, "\t\t\"speedup_vs_e1\": %.2f,\n", resultados_ciclos[i].speedup);
        fprintf(fp, "\t\t\"tiempo_muerto_kernel\": %.6f,\n", resultados_ciclos[i].tiempo_muerto_kernel);
        fprintf(fp, "\t\t\"procesos\": {\n");

        escribir_json_stats(fp, "proceso1", &resultados_ciclos[i].p1_stats, 1);
        escribir_json_stats(fp, "proceso2", &resultados_ciclos[i].p2_stats, 0);
        escribir_json_stats(fp, "proceso3", &resultados_ciclos[i].p3_stats, 0);

        fprintf(fp, "\t\t}\n");
        fprintf(fp, "\t}");
    }

    fprintf(fp, "\n]");
    fclose(fp);

    printf(COLOR_KERNEL "\n[Control Central] " ANSI_RESET "REPORTE JSON COMPLETO: Se han exportado %d ciclos a 'metricas_mision.json'.\n" ANSI_RESET, indice_resultados);
    indice_resultados = 0;
}

void almacenar_resultado_ciclo(double tiempo_total_ciclo, double speedup)
{
    if (indice_resultados >= CICLOS_POR_REPORTE)
        return;

    CicloResultado *res = &resultados_ciclos[indice_resultados];
    res->ciclo = ciclo_actual - 1;
    res->escenario = escenario_actual;
    res->tiempo_total_ciclo = tiempo_total_ciclo;
    res->speedup = speedup;

    res->tiempo_muerto_kernel = res->tiempo_total_ciclo - (time_p1 + time_p2 + time_p3);

    res->p1_stats = p1_full_stats;
    res->p2_stats = p2_full_stats;
    res->p3_stats = p3_full_stats;

    indice_resultados++;

    acumular_metricas_ciclo(tiempo_total_ciclo);

    if (indice_resultados == CICLOS_POR_REPORTE)
    {
        exportar_resultados_a_json();
        exportar_reporte_acumulado_a_json();
        imprimir_reporte_acumulado();
    }
}

int main()
{
    printf(COLOR_KERNEL "[Centro de Control] Iniciando Orquestador de Misión." ANSI_RESET "\n");
    printf(COLOR_YELLOW "El ciclo de monitoreo se repetirá cada 10 segundos.\n Presione Ctrl + Z para reiniciar y seleccionar un nuevo protocolo" ANSI_RESET "\n");

    signal(SIGTSTP, reiniciar_escenario);

    while (1)
    {

        if (escenario_actual == 0)
        {
            printf("Seleccione el protocolo de ejecución (1-4): ");
            if (scanf("%d", &escenario_actual) != 1 || escenario_actual < 1 || escenario_actual > 4)
                escenario_actual = 0;
            while (fgetc(stdin) != '\n')
                ;
        }

        if (escenario_actual != 0)
        {
            printf(COLOR_CICLO "\n--- Inicio del ciclo #%d ---\n" ANSI_RESET, ciclo_actual);

            gettimeofday(&ciclo_start, NULL);

            inicializar_ciclo();

            printf(COLOR_KERNEL "\n[Control Central] " ANSI_RESET "Iniciando protocolo de escenario: %d..." ANSI_RESET "\n", escenario_actual);

            ejecutar_escenario();
        }

        mostrar_tabla_recursos();

        if (escenario_actual == 2 || escenario_actual == 3)
        {
            imprimir_metricas_rr();
        }

        struct timeval ciclo_end;

        gettimeofday(&ciclo_end, NULL);

        double tiempo_total_ciclo = timeval_diff(&ciclo_start, &ciclo_end);
        double speedup = 0.0;

        printf(COLOR_CICLO "Tiempo total del ciclo: %.6f segundos\n" ANSI_RESET, tiempo_total_ciclo);

        if (escenario_actual == 2)
        {
            if (tiempo_escenario_2 == 0.0)
            {
                tiempo_escenario_2 = tiempo_total_ciclo;
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Tiempo base (Escenario 2 establecido: %.6f s\n" ANSI_RESET, tiempo_escenario_2);
            }
        }

        else if (escenario_actual != 2 && tiempo_escenario_2 > 0.0)
        {
            speedup = tiempo_escenario_2 / tiempo_total_ciclo;
            printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Mejora (SpeedUp) vs Escenario 2: %.2fx\n" ANSI_RESET, speedup);
        }

        almacenar_resultado_ciclo(tiempo_total_ciclo, speedup);

        printf(COLOR_CICLO "--- Fin de ciclo #%d ---\n" ANSI_RESET, ciclo_actual++);

        sleep(10);
    }

    return 0;
}