#include <stdio.h>
#include <string.h>

#define NUM_MATERIA 5
#define MAX_ALUMNOS 100

struct Alumno {
    char nombre[50];
    char grupo[20];
    int edad;
    float calificaciones[NUM_MATERIA];
    float promedio;
};

void ordenarPorPromedio(struct Alumno alumnos[], int n) {
    struct Alumno temp;
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - 1 - i; j++) {
            if (alumnos[j].promedio < alumnos[j + 1].promedio) {
                temp = alumnos[j];
                alumnos[j] = alumnos[j + 1];
                alumnos[j + 1] = temp;
            }
        }
    }
}

void ordenarPorEdad(struct Alumno alumnos[], int n) {
    struct Alumno temp;
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - 1 - i; j++) {
            if (alumnos[j].edad > alumnos[j + 1].edad) {
                temp = alumnos[j];
                alumnos[j] = alumnos[j + 1];
                alumnos[j + 1] = temp;
            }
        }
    }
}

int main() {

    struct Alumno alumnos[MAX_ALUMNOS];
    int cantidad;

    printf("¿Cuantos alumnos desea registrar? ");
    scanf("%d", &cantidad);
    getchar();

    for (int i = 0; i < cantidad; i++) {

        printf("\n===== Alumno %d =====\n", i + 1);

        printf("Nombre: ");
        fgets(alumnos[i].nombre, sizeof(alumnos[i].nombre), stdin);
        alumnos[i].nombre[strcspn(alumnos[i].nombre, "\n")] = '\0';

        printf("Grupo: ");
        fgets(alumnos[i].grupo, sizeof(alumnos[i].grupo), stdin);
        alumnos[i].grupo[strcspn(alumnos[i].grupo, "\n")] = '\0';

        printf("Edad: ");
        scanf("%d", &alumnos[i].edad);

        float suma = 0.0;
        for (int j = 0; j < NUM_MATERIA; j++) {
            printf("Calificacion %d: ", j + 1);
            scanf("%f", &alumnos[i].calificaciones[j]);
            suma += alumnos[i].calificaciones[j];
        }

        alumnos[i].promedio = suma / NUM_MATERIA;
        getchar();
    }

    // Ordenar por promedio (mayor a menor)
    ordenarPorPromedio(alumnos, cantidad);

    printf("\n===== LISTA ORDENADA POR CALIFICACION =====\n\n");

    for (int i = 0; i < cantidad; i++) {
        printf("Alumno: %s | Calificacion: %.2f | Grupo: %s\n",
               alumnos[i].nombre,
               alumnos[i].promedio,
               alumnos[i].grupo);
    }

    // Alumno con mayor y menor calificación
    printf("\nEl alumno con mayor calificacion es %s\n", alumnos[0].nombre);
    printf("El alumno mas bajo de calificacion es %s\n",
           alumnos[cantidad - 1].nombre);

    // Menú para ordenar por edad
    int opcion;
    printf("\n===== MENU =====\n");
    printf("1. Ordenar alumnos por edad (menor a mayor)\n");
    printf("2. Salir\n");
    printf("Seleccione una opcion: ");
    scanf("%d", &opcion);

    if (opcion == 1) {

        ordenarPorEdad(alumnos, cantidad);

        printf("\n===== LISTA ORDENADA POR EDAD =====\n\n");

        for (int i = 0; i < cantidad; i++) {
            printf("Alumno: %s | Edad: %d | Calificacion: %.2f | Grupo: %s\n",
                   alumnos[i].nombre,
                   alumnos[i].edad,
                   alumnos[i].promedio,
                   alumnos[i].grupo);
        }
    }

    return 0;
}
