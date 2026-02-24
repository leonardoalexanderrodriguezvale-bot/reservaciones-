#include <stdio.h>
#include <stdlib.h>  // Para abs()

int main() {

    int x1, x2, x3;
    int valido = 0;

    // ===== Validar x1 =====
    while (!valido) {
        printf("Ingrese el primer numero entero (x1): ");
        
        if (scanf("%d", &x1) != 1) {
            while (getchar() != '\n');  // limpiar buffer
            printf("Error: Debe ingresar un numero entero.\n");
        } else {
            valido = 1;
        }
    }

    // ===== Validar x2 =====
    valido = 0;
    while (!valido) {
        printf("Ingrese el segundo numero entero (x2): ");
        
        if (scanf("%d", &x2) != 1) {
            while (getchar() != '\n');
            printf("Error: Debe ingresar un numero entero.\n");
        } else {
            valido = 1;
        }
    }

    // ===== Validar x3 =====
    valido = 0;
    while (!valido) {
        printf("Ingrese el tercer numero entero (x3): ");
        
        if (scanf("%d", &x3) != 1) {
            while (getchar() != '\n');
            printf("Error: Debe ingresar un numero entero.\n");
        } else {
            valido = 1;
        }
    }

    // ===== Comparaciones =====
    printf("\n--- Resultados de las comparaciones ---\n");

    if (x1 == x2 && x2 == x3)
        printf("Los tres numeros son iguales.\n");
    else
        printf("Los tres numeros NO son iguales.\n");

    if (x1 > x2)
        printf("x1 es mayor a x2.\n");
    else
        printf("x1 NO es mayor a x2.\n");

    if (x1 > x3)
        printf("x1 es mayor a x3.\n");
    else
        printf("x1 NO es mayor a x3.\n");

    if (x2 == x3)
        printf("x2 es igual a x3.\n");
    else
        printf("x2 NO es igual a x3.\n");

    if (x2 > x1)
        printf("x2 es mayor a x1.\n");
    else
        printf("x2 NO es mayor a x1.\n");

    // ===== Cálculo del módulo (valor absoluto) =====
    printf("\n--- Modulos (valor absoluto) ---\n");
    printf("Modulo de x1: %d\n", abs(x1));
    printf("Modulo de x2: %d\n", abs(x2));
    printf("Modulo de x3: %d\n", abs(x3));

    return 0;
}
