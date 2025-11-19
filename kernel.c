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

static int escenario_actual = 0;
static int ciclo_actual = 1;

static struct rusage usage_p1, usage_p2, usage_p3;
static pid_t pid_p1 = 0, pid_p2 = 0, pid_p3 = 0;
static struct timeval p1_start, p2_start, p3_start, ciclo_start;
static double time_p1 = 0, time_p2 = 0, time_p3 = 0;
static double tiempo_escenario_1 = 0.0;

void guardar_stats_proceso(const char *nombre_proceso, pid_t pid, double wall_time, struct rusage *usage)
{
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

double timeval_diff(struct timeval *start, struct timeval *end)
{
    return (end->tv_sec - start->tv_sec) +
           (end->tv_usec - start->tv_usec) / 1000000.0;
}

void inicializar_ciclo()
{
    pid_p1 = pid_p2 = pid_p3 = 0;
    time_p1 = time_p2 = time_p3 = 0.0;
    memset(&usage_p1, 0, sizeof(struct rusage));
    memset(&usage_p2, 0, sizeof(struct rusage));
    memset(&usage_p3, 0, sizeof(struct rusage));
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
void esperar_proceso(pid_t pid, const char *nombre_proceso, int timeout_sec, struct timeval *start_time)
{
    int status;
    struct rusage usage;
    struct timeval end_time;
    double wall_time = 0.0;

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
    guardar_stats_proceso(nombre_proceso, pid, wall_time, &usage);
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
    else
        printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "No se recibió análisis final de %s%s" COLOR_KERNEL "." ANSI_RESET "\n",
               color_proceso("./proceso3"), nombre_legible("./proceso3"));

    close(pipe_fd);
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
    kill(pid3, SIGSTOP);

    esperar_proceso(pid1, "./code/escenariosBasicos/proceso1", 0, &p1_start);

    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Receptor (P1) terminó. Activando %s%s" COLOR_KERNEL "..." ANSI_RESET "\n", color_proceso("proceso2"), nombre_legible("proceso2"));

    gettimeofday(&p2_start, NULL);
    kill(pid2, SIGCONT);
    esperar_proceso(pid2, "./code/escenariosBasicos/proceso2", 0, &p2_start);

    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Escudo (P2) terminó. Activando %s%s" COLOR_KERNEL "..." ANSI_RESET "\n", color_proceso("proceso3"), nombre_legible("proceso3"));

    gettimeofday(&p3_start, NULL);
    kill(pid3, SIGCONT);
    esperar_proceso(pid3, "./code/escenariosBasicos/proceso3", TIMEOUT_P3, &p3_start);

    leer_datos_p3(datos_pipe_p3[0]);
}

void ejecutar_escenario_2()
{
    pid_t pid1, pid2, pid3;
    const int QUANTUM_P1 = 3;
    const int QUANTUM_P3 = 3;

    int p1_input_pipe[2];
    int p1_to_p3_pipe[2];
    int p3_to_kernel_pipe[2];

    struct rusage usage;
    int p1_status, p3_status;
    int p1_vivo = 1, p3_vivo = 1;
    int last_temp = -1;
    double tiempo_acumulado_p1 = 0.0;
    double tiempo_acumulado_p3 = 0.0;
    struct timeval turno_start, turno_end;

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
    kill(pid3, SIGSTOP);

    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Procesos listos. Iniciando...\n");

    enviar_contenido_archivo_a_pipe(p1_input_pipe[1], "medidas.txt");
    close(p1_input_pipe[1]);

    while (p1_vivo || p3_vivo)
    {

        if (p1_vivo)
        {
            printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Activando %s%s (PID %d) por %d seg...\n",
                   color_proceso("./proceso1"), nombre_legible("./proceso1"), pid1, QUANTUM_P1);
            gettimeofday(&turno_start, NULL);
            kill(pid1, SIGCONT);
            sleep(QUANTUM_P1);
            gettimeofday(&turno_end, NULL);
            tiempo_acumulado_p1 += timeval_diff(&turno_start, &turno_end);

            if (wait4(pid1, &p1_status, WNOHANG, &usage) == pid1)
            {
                p1_vivo = 0;
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "%s%s (PID %d) terminó." ANSI_RESET "\n",
                       color_proceso("./proceso1"), nombre_legible("./proceso1"), pid1);
                guardar_stats_proceso("./code/escenariosBasicos/proceso1", pid1, tiempo_acumulado_p1, &usage);
            }
            else
            {
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Deteniendo %s%s (PID %d). Tiempo agotado.\n",
                       color_proceso("./proceso1"), nombre_legible("./proceso1"), pid1);
                kill(pid1, SIGSTOP);
            }
        }

        if (p3_vivo)
        {
            printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Activando %s%s (PID %d) por %d seg...\n",
                   color_proceso("./proceso3"), nombre_legible("./proceso3"), pid3, QUANTUM_P3);
            gettimeofday(&turno_start, NULL);
            kill(pid3, SIGCONT);
            sleep(QUANTUM_P3);

            last_temp = leer_datos_p3(p3_to_kernel_pipe[0]);

            printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Temperatura analizada por %s%s: %d\n",
                   color_proceso("./proceso3"), nombre_legible("./proceso3"), last_temp);

            gettimeofday(&turno_end, NULL);
            tiempo_acumulado_p3 += timeval_diff(&turno_start, &turno_end);

            if (wait4(pid3, &p3_status, WNOHANG, &usage) == pid3)
            {
                p3_vivo = 0;
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "%s%s (PID %d) terminó." ANSI_RESET "\n",
                       color_proceso("./proceso3"), nombre_legible("./proceso3"), pid3);
                guardar_stats_proceso("./code/escenariosBasicos/proceso3", pid3, tiempo_acumulado_p3, &usage);
            }
            else
            {
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Deteniendo %s%s (PID %d). Tiempo agotado.\n",
                       color_proceso("./proceso3"), nombre_legible("./proceso3"), pid3);
                kill(pid3, SIGSTOP);
            }
        }

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

    close(p3_to_kernel_pipe[0]);

    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Ciclo de Round-Robin finalizado.\n");
}

void ejecutar_escenario_3()
{
    pid_t pid1, pid2, pid3;
    const int TIMEOUT_P1 = 10;
    const int TIMEOUT_P3 = 5;

    int p1_input_pipe[2], p1_to_p3_pipe[2], p3_to_kernel_pipe[2];

    struct rusage usage;
    int p1_status, p3_status;
    int p1_vivo = 1, p3_vivo = 1;
    int arg_p2_proximo = -1;

    double tiempo_acumulado_p1 = 0.0;
    double tiempo_acumulado_p3 = 0.0;
    struct timeval turno_start, turno_end;

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
    kill(pid3, SIGSTOP);

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

            gettimeofday(&turno_start, NULL);
            kill(pid1, SIGCONT);
            sleep(TIMEOUT_P1);

            gettimeofday(&turno_end, NULL);
            tiempo_acumulado_p1 += timeval_diff(&turno_start, &turno_end);

            if (wait4(pid1, &p1_status, WNOHANG, &usage) == pid1)
            {
                p1_vivo = 0;
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "%s%s (PID %d) terminó.\n",
                       color_proceso("./proceso1"), nombre_legible("./proceso1"), pid1);
                guardar_stats_proceso("./code/escenariosBasicos/proceso1", pid1, tiempo_acumulado_p1, &usage);
            }
            else
            {
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Deteniendo %s%s (PID %d). Tiempo agotado.\n",
                       color_proceso("./proceso1"), nombre_legible("./proceso1"), pid1);
                kill(pid1, SIGSTOP);
            }
        }

        if (p3_vivo)
        {
            printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Activando %s%s (PID %d) por %ds...\n",
                   color_proceso("./proceso3"), nombre_legible("./proceso3"), pid3, TIMEOUT_P3);

            gettimeofday(&turno_start, NULL);
            kill(pid3, SIGCONT);
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

            if (wait4(pid3, &p3_status, WNOHANG, &usage) == pid3)
            {
                p3_vivo = 0;
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "%s%s (PID %d) terminó.\n",
                       color_proceso("./proceso3"), nombre_legible("./proceso3"), pid3);
                guardar_stats_proceso("./code/escenariosBasicos/proceso3", pid3, tiempo_acumulado_p3, &usage);

                if (p1_vivo)
                {
                    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Terminó %s%s. Forzando terminación de %s%s (PID %d)...\n",
                           color_proceso("./proceso3"), nombre_legible("./proceso3"),
                           color_proceso("./proceso1"), nombre_legible("./proceso1"), pid1);
                    kill(pid1, SIGKILL);
                    wait4(pid1, &p1_status, 0, NULL);
                    p1_vivo = 0;
                }
            }
            else
            {
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Deteniendo %s%s (PID %d). Tiempo agotado.\n",
                       color_proceso("./proceso3"), nombre_legible("./proceso3"), pid3);
                kill(pid3, SIGSTOP);
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
        printf(COLOR_CICLO "--- Protocolo 1: [Receptor] -> [Escudo] -> [Analizador] ---\n" ANSI_RESET);
        ejecutar_escenario_1();
        break;
    case 2:
        printf(COLOR_CICLO "--- Protocolo 2: [Receptor] -> [Analizador] -> [Escudo] ---\n" ANSI_RESET);
        ejecutar_escenario_2();
        break;
    case 3:
        printf(COLOR_CICLO "--- Protocolo 3: [Escudo] -> [Receptor] -> [Analizador] ---\n" ANSI_RESET);
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

void reiniciar_escenario()
{
    system("clear");
    printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Reinicio de protocolos solicitado (SIGTSTP). Seleccione nuevo protocolo." ANSI_RESET "\n");
    escenario_actual = 0;
    ciclo_actual = 1;
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

        struct timeval ciclo_end;

        gettimeofday(&ciclo_end, NULL);

        double tiempo_total_ciclo = timeval_diff(&ciclo_start, &ciclo_end);

        printf(COLOR_CICLO "Tiempo total del ciclo: %.6f segundos\n" ANSI_RESET, tiempo_total_ciclo);

        if (escenario_actual == 1)
        {
            if (tiempo_escenario_1 == 0.0)
            {
                tiempo_escenario_1 = tiempo_total_ciclo;
                printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Tiempo base (Protocolo Secuencial 1) establecido: %.6f s\n" ANSI_RESET, tiempo_escenario_1);
            }
        }

        else if (escenario_actual > 1 && tiempo_escenario_1 > 0.0)
        {
            double speedup = tiempo_escenario_1 / tiempo_total_ciclo;
            printf(COLOR_KERNEL "[Control Central] " ANSI_RESET "Mejora (SpeedUp) vs Protocolo 1: %.2fx\n" ANSI_RESET, speedup);
        }

        printf(COLOR_CICLO "--- Fin de ciclo #%d ---\n" ANSI_RESET, ciclo_actual++);

        sleep(10);
    }

    return 0;
}