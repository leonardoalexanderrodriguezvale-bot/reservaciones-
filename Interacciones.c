#include <stdio.h>

int main() 
{
    int numero, iteraciones;
    int condiciones = 0;

    while (condiciones != 2)
    {
        condiciones = 0; // Reiniciar contador

        printf("\n:: Generador de Tablas de Multiplicar ::\n");

        // ===== Validar numero =====
        printf("Ingrese un numero entero: ");
        if (scanf("%d", &numero) != 1)
        {
            while (getchar() != '\n'); // limpiar buffer
            printf("Entrada invalida. Debe ingresar un numero entero.\n");
        }
        else
        {
            printf("El numero %d es entero.\n", numero);
            condiciones++;
        }

        // ===== Validar iteraciones =====
        printf("Ingrese la cantidad de iteraciones: ");
        if (scanf("%d", &iteraciones) != 1 || iteraciones <= 0)
        {
            while (getchar() != '\n');
            printf("Entrada invalida. Debe ingresar un numero entero positivo.\n");
        }
        else
        {
            printf("La cantidad de iteraciones %d es valida.\n", iteraciones);
            condiciones++;
        }

        if (condiciones != 2)
        {
            printf("\nError en los datos. Intente nuevamente.\n");
        }
    }

    // ===== Tabla de multiplicar en bloques de 10 =====
    printf("\nTabla de multiplicar del %d\n", numero);
    printf("=================================================\n");

    for (int i = 1; i <= iteraciones; i++)
    {
        // Cada 10 inicia una nueva tabla
        if ((i - 1) % 10 == 0)
        {
            printf("\n--- Tabla del %d (del %d al %d) ---\n",
                   numero,
                   i,
                   (i + 9 <= iteraciones) ? i + 9 : iteraciones);
        }

        int resultado = numero * i;
        printf("%2d x %2d = %3d\n", numero, i, resultado);

        // Separador al terminar cada bloque de 10
        if (i % 10 == 0)
        {
            printf("-----------------------------------\n");
        }
    }

    printf("=================================================\n");

    return 0;
}
