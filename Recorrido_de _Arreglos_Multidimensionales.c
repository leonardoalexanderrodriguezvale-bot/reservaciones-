#include <stdio.h>
#include <string.h>

#define ALUMNOS 5
#define PARCIALES 3

typedef struct {
    char nombre[50];
    char grupo[20];
    float calificaciones[PARCIALES];
    float promedio;
} Alumno;

int main() {
    Alumno alumnos[ALUMNOS];
    float promedioParcial[PARCIALES] = {0};
    
    // Cargar datos de alumnos
    for (int i = 0; i < ALUMNOS; i++) {
        printf("\n--- Alumno %d ---\n", i + 1);
        printf("Nombre: ");
        fgets(alumnos[i].nombre, sizeof(alumnos[i].nombre), stdin);
        alumnos[i].nombre[strcspn(alumnos[i].nombre, "\n")] = 0;
        
        printf("Grupo: ");
        fgets(alumnos[i].grupo, sizeof(alumnos[i].grupo), stdin);
        alumnos[i].grupo[strcspn(alumnos[i].grupo, "\n")] = 0;
        
        alumnos[i].promedio = 0;
        for (int j = 0; j < PARCIALES; j++) {
            printf("Calificacion Parcial %d: ", j + 1);
            scanf("%f", &alumnos[i].calificaciones[j]);
            alumnos[i].promedio += alumnos[i].calificaciones[j];
            promedioParcial[j] += alumnos[i].calificaciones[j];
        }
        alumnos[i].promedio /= PARCIALES;
        getchar();
    }
    
    // Calcular promedios por parcial
    for (int j = 0; j < PARCIALES; j++) {
        promedioParcial[j] /= ALUMNOS;
    }
    
    // Mostrar matriz de calificaciones
    printf("\n========== MATRIZ DE CALIFICACIONES ==========\n");
    printf("%-20s %-10s %-12s %-12s %-12s %-12s\n", 
           "Nombre", "Grupo", "Parcial 1", "Parcial 2", "Parcial 3", "Promedio");
    printf("---------------------------------------------------------------\n");
    
    for (int i = 0; i < ALUMNOS; i++) {
        printf("%-20s %-10s %-12.2f %-12.2f %-12.2f %-12.2f\n",
               alumnos[i].nombre, alumnos[i].grupo,
               alumnos[i].calificaciones[0], alumnos[i].calificaciones[1],
               alumnos[i].calificaciones[2], alumnos[i].promedio);
    }
    
    printf("---------------------------------------------------------------\n");
    printf("%-20s %-10s %-12.2f %-12.2f %-12.2f\n",
           "PROMEDIO PARCIAL", "",
           promedioParcial[0], promedioParcial[1], promedioParcial[2]);
    
    return 0;
}
