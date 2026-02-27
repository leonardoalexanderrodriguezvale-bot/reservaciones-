/*
 ============================================================
  SISTEMA DE GESTION DE CALIFICACIONES - ARREGLO 3D
  Dimensiones: [PARCIAL][MATERIA][ALUMNO]
  Parciales fijos : 3
  Materias y Alumnos: se piden al inicio del programa
 ============================================================
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ---------- Constantes ---------- */
#define MAX_PARCIALES  3
#define CAL_MIN        0.0f
#define CAL_MAX        10.0f
#define APROBATORIO    6.0f
#define TAM_NOMBRE     40
#define TAM_BUF        64
#define MAX_MAT_ALU    50

/* ---------- Variables globales ---------- */
int    numMaterias = 0;
int    numAlumnos  = 0;

char (*nombresAlumnos) [TAM_NOMBRE] = NULL;
char (*nombresMaterias)[TAM_NOMBRE] = NULL;
float ***cal = NULL;

int datosOk = 0;
int calOk   = 0;

const char etiq_p[MAX_PARCIALES][4] = {"P1", "P2", "P3"};

/* ===========================================================
   Libera la memoria del arreglo 3D
   =========================================================== */
void liberarCal(void)
{
    int p, m;
    if (cal == NULL) return;
    for (p = 0; p < MAX_PARCIALES; p++)
    {
        if (cal[p] != NULL)
        {
            for (m = 0; m < numMaterias; m++)
                free(cal[p][m]);
            free(cal[p]);
        }
    }
    free(cal);
    cal = NULL;
}

/* ===========================================================
   Lee una linea de stdin de forma segura.
   =========================================================== */
int leerLinea(char *buf, int tam)
{
    int n;
    if (fgets(buf, tam, stdin) == NULL) { buf[0] = '\0'; return 0; }
    n = (int)strlen(buf);
    if (n > 0 && buf[n - 1] == '\n') { buf[n - 1] = '\0'; n--; }
    return n;
}

/* ===========================================================
   Lee un float validado en [minV, maxV]
   =========================================================== */
float leerFloat(float minV, float maxV)
{
    char  buf[TAM_BUF];
    float v;
    int   ok = 0;
    while (!ok)
    {
        leerLinea(buf, TAM_BUF);
        if (sscanf(buf, "%f", &v) == 1 && v >= minV && v <= maxV) ok = 1;
        else printf("    Valor invalido. Ingrese entre %.1f y %.1f: ", minV, maxV);
    }
    return v;
}

/* ===========================================================
   Lee un entero en [1, limite]. Retorna base-0.
   =========================================================== */
int leerIndice(int limite)
{
    char buf[TAM_BUF];
    int  v, ok = 0;
    while (!ok)
    {
        leerLinea(buf, TAM_BUF);
        if (sscanf(buf, "%d", &v) == 1 && v >= 1 && v <= limite) ok = 1;
        else printf("    Indice invalido. Ingrese entre 1 y %d: ", limite);
    }
    return v - 1;
}

/* ===========================================================
   Lee una opcion de menu en [minOp, maxOp].
   =========================================================== */
int leerOpcion(int minOp, int maxOp)
{
    char buf[TAM_BUF];
    int  v, ok = 0;
    while (!ok)
    {
        leerLinea(buf, TAM_BUF);
        if (sscanf(buf, "%d", &v) == 1 && v >= minOp && v <= maxOp) ok = 1;
        else printf("    Opcion no valida. Ingrese entre %d y %d: ", minOp, maxOp);
    }
    return v;
}

/* ===========================================================
   CONFIGURACION INICIAL - Pide numero de materias y alumnos
   y asigna memoria dinamica.
   =========================================================== */
void configurar(void)
{
    int p, m, a;

    printf("\n=============================\n");
    printf(" CONFIGURACION INICIAL\n");
    printf("=============================\n");

    printf("  Numero de materias [1-%d]: ", MAX_MAT_ALU);
    numMaterias = leerIndice(MAX_MAT_ALU) + 1;

    printf("  Numero de alumnos  [1-%d]: ", MAX_MAT_ALU);
    numAlumnos = leerIndice(MAX_MAT_ALU) + 1;

    /* Liberar memoria previa */
    if (nombresMaterias != NULL) { free(nombresMaterias); nombresMaterias = NULL; }
    if (nombresAlumnos  != NULL) { free(nombresAlumnos);  nombresAlumnos  = NULL; }
    liberarCal();

    /* Alocar nombres */
    nombresMaterias = (char (*)[TAM_NOMBRE]) malloc(numMaterias * TAM_NOMBRE * sizeof(char));
    nombresAlumnos  = (char (*)[TAM_NOMBRE]) malloc(numAlumnos  * TAM_NOMBRE * sizeof(char));

    if (nombresMaterias == NULL || nombresAlumnos == NULL)
    {
        printf("  [ERROR] Sin memoria suficiente.\n");
        exit(1);
    }

    /* Nombres por defecto */
    for (m = 0; m < numMaterias; m++) sprintf(nombresMaterias[m], "Materia%d", m + 1);
    for (a = 0; a < numAlumnos;  a++) sprintf(nombresAlumnos[a],  "Alumno%d",  a + 1);

    /* Alocar arreglo 3D */
    cal = (float ***) malloc(MAX_PARCIALES * sizeof(float **));
    for (p = 0; p < MAX_PARCIALES; p++)
    {
        cal[p] = (float **) malloc(numMaterias * sizeof(float *));
        for (m = 0; m < numMaterias; m++)
        {
            cal[p][m] = (float *) malloc(numAlumnos * sizeof(float));
            for (a = 0; a < numAlumnos; a++)
                cal[p][m][a] = 0.0f;
        }
    }

    datosOk = 0;
    calOk   = 0;
    printf("\n  [OK] Configurado: %d materias, %d alumnos.\n", numMaterias, numAlumnos);
}

/* ===========================================================
   OPCION 1 - Capturar nombres
   =========================================================== */
void capturarNombres(void)
{
    int  i;
    char buf[TAM_BUF];

    printf("\n--- NOMBRES DE ALUMNOS ---\n");
    for (i = 0; i < numAlumnos; i++)
    {
        printf("  Alumno %d: ", i + 1);
        leerLinea(buf, TAM_BUF);
        if ((int)strlen(buf) == 0) { printf("    Nombre vacio, intente de nuevo.\n"); i--; continue; }
        strncpy(nombresAlumnos[i], buf, TAM_NOMBRE - 1);
        nombresAlumnos[i][TAM_NOMBRE - 1] = '\0';
    }

    printf("\n--- NOMBRES DE MATERIAS ---\n");
    for (i = 0; i < numMaterias; i++)
    {
        printf("  Materia %d: ", i + 1);
        leerLinea(buf, TAM_BUF);
        if ((int)strlen(buf) == 0) { printf("    Nombre vacio, intente de nuevo.\n"); i--; continue; }
        strncpy(nombresMaterias[i], buf, TAM_NOMBRE - 1);
        nombresMaterias[i][TAM_NOMBRE - 1] = '\0';
    }

    datosOk = 1;
    printf("\n  [OK] Nombres guardados.\n");
}

/* ===========================================================
   OPCION 2 - Capturar calificaciones
   =========================================================== */
void capturarCalificaciones(void)
{
    int p, m, a;
    if (!datosOk) { printf("\n  [!] Capture primero los nombres (Opcion 1).\n"); return; }

    printf("\n--- CAPTURA DE CALIFICACIONES ---\n");
    for (p = 0; p < MAX_PARCIALES; p++)
    {
        printf("\n  == Parcial %s ==\n", etiq_p[p]);
        for (m = 0; m < numMaterias; m++)
        {
            printf("  Materia: %s\n", nombresMaterias[m]);
            for (a = 0; a < numAlumnos; a++)
            {
                printf("    %-25s : ", nombresAlumnos[a]);
                cal[p][m][a] = leerFloat(CAL_MIN, CAL_MAX);
            }
        }
    }
    calOk = 1;
    printf("\n  [OK] Calificaciones guardadas.\n");
}

/* ===========================================================
   OPCION 3 - Mostrar tabla
   =========================================================== */
void mostrarTabla(void)
{
    int   p, m, a;
    float suma, prom;
    char  estado;

    if (!calOk) { printf("\n  [!] Capture primero las calificaciones (Opcion 2).\n"); return; }

    for (p = 0; p < MAX_PARCIALES; p++)
    {
        printf("\n==============================\n");
        printf(" Parcial %s\n", etiq_p[p]);
        printf("==============================\n");
        printf("%-25s", "Alumno");
        for (m = 0; m < numMaterias; m++) printf("  %-10s", nombresMaterias[m]);
        printf("  Prom   Est\n");

        for (a = 0; a < numAlumnos; a++)
        {
            suma = 0.0f;
            printf("%-25s", nombresAlumnos[a]);
            for (m = 0; m < numMaterias; m++) { printf("  %-10.2f", cal[p][m][a]); suma += cal[p][m][a]; }
            prom   = suma / numMaterias;
            estado = (prom >= APROBATORIO) ? 'A' : 'R';
            printf("  %5.2f  %c\n", prom, estado);
        }
    }
}

/* ===========================================================
   OPCION 4 - Modificar calificacion
   =========================================================== */
void modificarCalificacion(void)
{
    int p, m, a;
    if (!calOk) { printf("\n  [!] Capture primero las calificaciones (Opcion 2).\n"); return; }

    printf("\n--- MODIFICAR CALIFICACION ---\n");
    printf("  Parcial  [1-%d]: ", MAX_PARCIALES); p = leerIndice(MAX_PARCIALES);
    printf("  Materia  [1-%d]: ", numMaterias);   m = leerIndice(numMaterias);
    printf("  Alumno   [1-%d]: ", numAlumnos);    a = leerIndice(numAlumnos);

    printf("  Actual de %s en %s (%s): %.2f\n",
           nombresAlumnos[a], nombresMaterias[m], etiq_p[p], cal[p][m][a]);
    printf("  Nueva calificacion: ");
    cal[p][m][a] = leerFloat(CAL_MIN, CAL_MAX);
    printf("  [OK] Actualizada a %.2f\n", cal[p][m][a]);
}

/* ===========================================================
   OPCION 5 - Promedios por alumno y por materia
   =========================================================== */
void calcularPromedios(void)
{
    int   p, m, a;
    float suma, prom;
    char  estado;

    if (!calOk) { printf("\n  [!] Capture primero las calificaciones (Opcion 2).\n"); return; }

    for (p = 0; p < MAX_PARCIALES; p++)
    {
        printf("\n==============================\n");
        printf(" PROMEDIOS - Parcial %s\n", etiq_p[p]);
        printf("==============================\n");

        printf("\n  Por alumno:\n");
        for (a = 0; a < numAlumnos; a++)
        {
            suma = 0.0f;
            for (m = 0; m < numMaterias; m++) suma += cal[p][m][a];
            prom = suma / numMaterias;
            estado = (prom >= APROBATORIO) ? 'A' : 'R';
            printf("    %-25s  %.2f  [%c]\n", nombresAlumnos[a], prom, estado);
        }

        printf("\n  Por materia:\n");
        for (m = 0; m < numMaterias; m++)
        {
            suma = 0.0f;
            for (a = 0; a < numAlumnos; a++) suma += cal[p][m][a];
            prom = suma / numAlumnos;
            estado = (prom >= APROBATORIO) ? 'A' : 'R';
            printf("    %-25s  %.2f  [%c]\n", nombresMaterias[m], prom, estado);
        }
    }
}

/* ===========================================================
   OPCION 6 - Maximo y minimo por parcial
   =========================================================== */
void reporteMaxMin(void)
{
    int   p, m, a, maxM, maxA, minM, minA;
    float maxV, minV, v;

    if (!calOk) { printf("\n  [!] Capture primero las calificaciones (Opcion 2).\n"); return; }

    printf("\n--- REPORTE MAXIMO Y MINIMO ---\n");
    for (p = 0; p < MAX_PARCIALES; p++)
    {
        maxV = -1.0f; minV = 11.0f;
        maxM = 0; maxA = 0; minM = 0; minA = 0;

        for (m = 0; m < numMaterias; m++)
            for (a = 0; a < numAlumnos; a++)
            {
                v = cal[p][m][a];
                if (v > maxV) { maxV = v; maxM = m; maxA = a; }
                if (v < minV) { minV = v; minM = m; minA = a; }
            }

        printf("\n  Parcial %s:\n", etiq_p[p]);
        printf("    MAX: %.2f -> %s / %s\n", maxV, nombresMaterias[maxM], nombresAlumnos[maxA]);
        printf("    MIN: %.2f -> %s / %s\n", minV, nombresMaterias[minM], nombresAlumnos[minA]);
    }
}

/* ===========================================================
   MAIN - Menu principal con do-while
   =========================================================== */
int main(void)
{
    int opcion;

    printf("Bienvenido al Sistema de Calificaciones\n");
    configurar();

    do
    {
        printf("\n=============================\n");
        printf(" SISTEMA DE CALIFICACIONES  \n");
        printf(" Materias: %d  |  Alumnos: %d\n", numMaterias, numAlumnos);
        printf("=============================\n");
        printf(" 1. Capturar nombres\n");
        printf(" 2. Capturar calificaciones\n");
        printf(" 3. Mostrar tabla\n");
        printf(" 4. Modificar calificacion\n");
        printf(" 5. Calcular promedios\n");
        printf(" 6. Reporte maximo/minimo\n");
        printf(" 7. Reconfigurar (cambiar no. materias/alumnos)\n");
        printf(" 0. Salir\n");
        printf("-----------------------------\n");
        printf(" Opcion: ");

        opcion = leerOpcion(0, 7);

        if      (opcion == 1) capturarNombres();
        else if (opcion == 2) capturarCalificaciones();
        else if (opcion == 3) mostrarTabla();
        else if (opcion == 4) modificarCalificacion();
        else if (opcion == 5) calcularPromedios();
        else if (opcion == 6) reporteMaxMin();
        else if (opcion == 7) configurar();
        else if (opcion == 0) printf("\n  Hasta luego!\n\n");

    } while (opcion != 0);

    liberarCal();
    free(nombresAlumnos);
    free(nombresMaterias);

    return 0;
}
