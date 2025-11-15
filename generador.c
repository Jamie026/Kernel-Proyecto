#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main()
{
    FILE *f = fopen("medidas.txt", "w");
    if (!f)
    {
        perror("Error creando medidas.txt");
        return 1;
    }

    srand(time(NULL));

    int activo = 1; // 1 = sistema encendido, 0 = apagado
    int temp = 45;

    for (int i = 0; i < 40; i++)
    {
        if (activo)
        {
            // Aumenta la temperatura en saltos grandes
            temp += (rand() % 40) + 10; // +10 a +50
            if (temp > 105)
                temp = 105;
        }
        else
        {
            // Disminuye en saltos grandes
            temp -= (rand() % 40) + 10; // -10 a -50
            if (temp < 45)
                temp = 45;
        }

        // Escribir temperatura al archivo
        fprintf(f, "%d\n", temp);

        // Lógica de activación / desactivación
        if (temp > 90)
            activo = 0; // Se apaga al superar 90
        if (temp < 55)
            activo = 1; // Se enciende al bajar de 55
    }

    fclose(f);
    printf("Archivo medidas.txt generado correctamente.\n");
    return 0;
}
