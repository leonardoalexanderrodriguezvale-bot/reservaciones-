/*
 * juego_completo.c
 * Juego de exploracion en consola con maquina de estados, guardado y trofeos.
 *
 * Compilar: gcc juego_completo.c -o juego
 * Ejecutar: ./juego
 *
 * Objetivo: recolectar 10 trofeos (*) para ganar.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <conio.h>
#include <windows.h>

/* ---------------------------------------------
   CONSTANTES
   --------------------------------------------- */

#define ALTO             10
#define ANCHO            20
#define ARCHIVO_GUARDADO "partida.dat"
#define MAGIC_NUMBER     0xDADB   /* incrementado para invalidar guardados viejos */
#define TROFEOS_PARA_GANAR 10
#define SIMBOLO_TROFEO   '*'

/* ---------------------------------------------
   ESTADOS
   --------------------------------------------- */

typedef enum {
    ESTADO_MENU,
    ESTADO_JUGANDO,
    ESTADO_VICTORIA,
    ESTADO_INSTRUCCIONES,
    ESTADO_DISENIO_MAPA,
    ESTADO_CONFIRMAR_SALIDA,
    ESTADO_CONFIRMAR_NUEVA,
    ESTADO_SALIR
} EstadoJuego;

/* ---------------------------------------------
   ESTRUCTURA PRINCIPAL
   --------------------------------------------- */

typedef struct {
    char        mapa[ALTO][ANCHO + 1];
    int         jugador_x;
    int         jugador_y;
    int         trofeo_x;          /* posicion actual del trofeo */
    int         trofeo_y;
    int         trofeos_recogidos; /* cuantos se han capturado */
    int         pasos;
    int         choques;
    char        ultima_tecla;
    char        ultimo_mensaje[64];
    EstadoJuego estado;
    int         partida_activa;
    int         hay_guardado;
} Juego;

/* Datos que se persisten en disco */
typedef struct {
    unsigned int magic;
    char         mapa[ALTO][ANCHO + 1];
    int          jugador_x;
    int          jugador_y;
    int          trofeo_x;
    int          trofeo_y;
    int          trofeos_recogidos;
    int          pasos;
    int          choques;
} DatosGuardado;

/* ---------------------------------------------
   MAPA BASE FIJO
   --------------------------------------------- */

static const char *MAPA_FIJO[ALTO] = {
    "####################",
    "#                  #",
    "#  #####           #",
    "#      #           #",
    "#      #   #####   #",
    "#          #       #",
    "#          #       #",
    "#   #####  #       #",
    "#                  #",
    "####################"
};

static char MAPA_CUSTOM[ALTO][ANCHO + 1];
static int  usar_mapa_custom = 0;

/* ---------------------------------------------
   CONSOLA (Windows)
   --------------------------------------------- */

static void consola_modo_raw(void)    {}
static void consola_modo_normal(void) {}

static char consola_leer_tecla(void) {
    int c = _getch();
    if (c == 0 || c == 0xE0) { _getch(); return 0; }
    return (char)c;
}

static void consola_limpiar(void) { system("cls"); }

/* Helpers de color */
static HANDLE hc_global = NULL;

static void color_set(WORD attr) {
    if (!hc_global) hc_global = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hc_global, attr);
}
static void color_reset(void) {
    color_set(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

/* ---------------------------------------------
   GUARDADO / CARGA
   --------------------------------------------- */

static int partida_guardar(const Juego *j) {
    FILE *f = fopen(ARCHIVO_GUARDADO, "wb");
    if (!f) return 0;
    DatosGuardado d;
    d.magic             = MAGIC_NUMBER;
    d.jugador_x         = j->jugador_x;
    d.jugador_y         = j->jugador_y;
    d.trofeo_x          = j->trofeo_x;
    d.trofeo_y          = j->trofeo_y;
    d.trofeos_recogidos = j->trofeos_recogidos;
    d.pasos             = j->pasos;
    d.choques           = j->choques;
    for (int y = 0; y < ALTO; y++) strncpy(d.mapa[y], j->mapa[y], ANCHO + 1);
    int ok = (fwrite(&d, sizeof(DatosGuardado), 1, f) == 1);
    fclose(f);
    return ok;
}

static int partida_cargar(Juego *j) {
    FILE *f = fopen(ARCHIVO_GUARDADO, "rb");
    if (!f) return 0;
    DatosGuardado d;
    int ok = (fread(&d, sizeof(DatosGuardado), 1, f) == 1);
    fclose(f);
    if (!ok || d.magic != MAGIC_NUMBER) return 0;
    j->jugador_x         = d.jugador_x;
    j->jugador_y         = d.jugador_y;
    j->trofeo_x          = d.trofeo_x;
    j->trofeo_y          = d.trofeo_y;
    j->trofeos_recogidos = d.trofeos_recogidos;
    j->pasos             = d.pasos;
    j->choques           = d.choques;
    for (int y = 0; y < ALTO; y++) strncpy(j->mapa[y], d.mapa[y], ANCHO + 1);
    j->partida_activa = 1;
    j->ultima_tecla   = ' ';
    strncpy(j->ultimo_mensaje, "Partida cargada!", sizeof(j->ultimo_mensaje) - 1);
    return 1;
}

static int partida_existe_guardado(void) {
    FILE *f = fopen(ARCHIVO_GUARDADO, "rb");
    if (!f) return 0;
    DatosGuardado d;
    int ok = (fread(&d, sizeof(DatosGuardado), 1, f) == 1);
    fclose(f);
    return (ok && d.magic == MAGIC_NUMBER);
}

/* ---------------------------------------------
   LOGICA DEL JUEGO
   --------------------------------------------- */

static int juego_es_pared(const Juego *j, int x, int y) {
    if (x < 0 || x >= ANCHO || y < 0 || y >= ALTO) return 1;
    return (j->mapa[y][x] == '#');
}

/* Coloca el trofeo en una celda libre aleatoria, distinta del jugador */
static void trofeo_colocar(Juego *j) {
    /* Recopilar todas las celdas libres */
    int celdas_x[ALTO * ANCHO];
    int celdas_y[ALTO * ANCHO];
    int n = 0;

    for (int y = 0; y < ALTO; y++) {
        for (int x = 0; x < ANCHO; x++) {
            if (!juego_es_pared(j, x, y) &&
                !(x == j->jugador_x && y == j->jugador_y)) {
                celdas_x[n] = x;
                celdas_y[n] = y;
                n++;
            }
        }
    }

    if (n == 0) return; /* no hay espacio (no deberia ocurrir) */

    int idx    = rand() % n;
    j->trofeo_x = celdas_x[idx];
    j->trofeo_y = celdas_y[idx];
}

static void juego_inicializar(Juego *j) {
    const char **fuente = usar_mapa_custom ? (const char **)MAPA_CUSTOM : MAPA_FIJO;
    for (int y = 0; y < ALTO; y++) {
        strncpy(j->mapa[y], fuente[y], ANCHO);
        j->mapa[y][ANCHO] = '\0';
    }

    j->jugador_x = 1;
    j->jugador_y = 1;
    if (juego_es_pared(j, j->jugador_x, j->jugador_y)) {
        for (int y = 1; y < ALTO - 1; y++) {
            for (int x = 1; x < ANCHO - 1; x++) {
                if (!juego_es_pared(j, x, y)) {
                    j->jugador_x = x; j->jugador_y = y; y = ALTO; break;
                }
            }
        }
    }

    j->pasos             = 0;
    j->choques           = 0;
    j->trofeos_recogidos = 0;
    j->ultima_tecla      = ' ';
    j->partida_activa    = 1;

    trofeo_colocar(j);  /* posicion aleatoria inicial del trofeo */

    strncpy(j->ultimo_mensaje, "!Busca el trofeo (*)!", sizeof(j->ultimo_mensaje) - 1);
}

static void juego_intentar_mover(Juego *j, int dx, int dy) {
    int nx = j->jugador_x + dx;
    int ny = j->jugador_y + dy;

    if (juego_es_pared(j, nx, ny)) {
        j->choques++;
        snprintf(j->ultimo_mensaje, sizeof(j->ultimo_mensaje),
                 "!Choque! Total: %d", j->choques);
        return;
    }

    j->jugador_x = nx;
    j->jugador_y = ny;
    j->pasos++;

    /* --- Comprobar si el jugador recoge el trofeo --- */
    if (nx == j->trofeo_x && ny == j->trofeo_y) {
        j->trofeos_recogidos++;

        if (j->trofeos_recogidos >= TROFEOS_PARA_GANAR) {
            /* Victoria: el trofeo desaparece, el estado cambia */
            j->trofeo_x = -1;
            j->trofeo_y = -1;
            j->estado   = ESTADO_VICTORIA;
        } else {
            /* Colocar nuevo trofeo en otra posicion aleatoria */
            trofeo_colocar(j);
            snprintf(j->ultimo_mensaje, sizeof(j->ultimo_mensaje),
                     "!Trofeo! Llevas %d/%d", j->trofeos_recogidos, TROFEOS_PARA_GANAR);
        }
    } else {
        snprintf(j->ultimo_mensaje, sizeof(j->ultimo_mensaje),
                 "Mov OK  Trofeos: %d/%d", j->trofeos_recogidos, TROFEOS_PARA_GANAR);
    }
}

/* ---------------------------------------------
   RENDERS
   --------------------------------------------- */

static void render_menu(const Juego *j) {
    consola_limpiar();
    puts("+================================+");
    puts("|     EXPLORADOR DE MAZMORRA     |");
    puts("|      Colecciona 10 trofeos!    |");
    puts("+================================+");
    puts("|  [1] Nueva partida             |");
    if (j->partida_activa)
        puts("|  [2] Continuar partida         |");
    else
        puts("|  [2] Continuar  (sin partida)  |");
    if (j->hay_guardado)
        puts("|  [3] Cargar partida guardada   |");
    else
        puts("|  [3] Cargar  (sin guardado)    |");
    puts("|  [4] Instrucciones             |");
    puts("|  [5] Disenar Mapa              |");
    puts("|  [q] Salir                     |");
    puts("+================================+");
    printf("\n  Elige una opcion: ");
    fflush(stdout);
}

static void render_juego(const Juego *j) {
    consola_limpiar();

    /* -- HUD -- */
    puts("+------------------------------------------+");
    printf("| Pasos: %-5d  Choques: %-4d  Pos:(%2d,%2d)  |\n",
           j->pasos, j->choques, j->jugador_x, j->jugador_y);

    /* Barra de progreso de trofeos */
    printf("| Trofeos: [");
    for (int i = 0; i < TROFEOS_PARA_GANAR; i++) {
        if (i < j->trofeos_recogidos) {
            color_set(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY); /* amarillo */
            putchar('*');
        } else {
            color_set(FOREGROUND_INTENSITY); /* gris */
            putchar('.');
        }
    }
    color_reset();
    printf("] %d/%d       |\n", j->trofeos_recogidos, TROFEOS_PARA_GANAR);

    printf("| %-42s |\n", j->ultimo_mensaje);
    puts("+------------------------------------------+");

    /* -- Mapa -- */
    for (int y = 0; y < ALTO; y++) {
        putchar(' ');
        for (int x = 0; x < ANCHO; x++) {
            if (x == j->jugador_x && y == j->jugador_y) {
                /* Jugador: amarillo brillante */
                color_set(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
                putchar('@');
                color_reset();
            } else if (x == j->trofeo_x && y == j->trofeo_y) {
                /* Trofeo: cian brillante */
                color_set(FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
                putchar(SIMBOLO_TROFEO);
                color_reset();
            } else if (j->mapa[y][x] == '#') {
                /* Pared: gris oscuro */
                color_set(FOREGROUND_INTENSITY);
                putchar('#');
                color_reset();
            } else {
                putchar(j->mapa[y][x]);
            }
        }
        putchar('\n');
    }

    puts("\n [w/a/s/d] Mover  |  [g] Guardar  |  [q] Menu  |  [i] Info");
    fflush(stdout);
}

static void render_victoria(const Juego *j) {
    consola_limpiar();
    color_set(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    puts("+========================================+");
    puts("|                                        |");
    puts("|        !!! FELICITACIONES !!!          |");
    puts("|                                        |");
    puts("|     Has recolectado los 10 trofeos!    |");
    puts("|                                        |");
    puts("+========================================+");
    color_reset();
    printf("|  Pasos totales  : %-5d                 |\n", j->pasos);
    printf("|  Choques totales: %-5d                 |\n", j->choques);
    puts("+========================================+");
    puts("\n  [cualquier tecla] Volver al menu...");
    fflush(stdout);
}

static void render_instrucciones(void) {
    consola_limpiar();
    puts("+========================================+");
    puts("|            INSTRUCCIONES               |");
    puts("+========================================+");
    puts("|  w/a/s/d -> Mover al jugador           |");
    puts("|  g / G   -> Guardar partida            |");
    puts("|  q / Q   -> Volver al menu             |");
    puts("|  i / I   -> Ver instrucciones          |");
    puts("+========================================+");
    puts("|  @  = tu personaje (amarillo)          |");
    puts("|  *  = trofeo a recoger (cian)          |");
    puts("|  #  = pared (no puedes cruzarla)       |");
    puts("|                                        |");
    puts("|  Recoge 10 trofeos para ganar.         |");
    puts("|  Cada trofeo aparece en lugar aleatorio|");
    puts("|  tras ser recogido.                    |");
    puts("+========================================+");
    puts("\n  [cualquier tecla] Volver...");
    fflush(stdout);
}

static void render_confirmar_salida(void) {
    consola_limpiar();
    puts("+================================+");
    puts("|   ?Volver al menu principal?   |");
    puts("+================================+");
    puts("|  [g] Guardar y volver al menu  |");
    puts("|  [s] Volver SIN guardar        |");
    puts("|  [n] Cancelar, seguir jugando  |");
    puts("+================================+");
    fflush(stdout);
}

static void render_confirmar_nueva(void) {
    consola_limpiar();
    puts("+================================+");
    puts("|   Hay una partida guardada.    |");
    puts("|   ?Iniciar nueva partida?      |");
    puts("|   Se perdera el avance.        |");
    puts("+================================+");
    puts("|  [s] Si, nueva partida         |");
    puts("|  [n] No, volver al menu        |");
    puts("+================================+");
    fflush(stdout);
}

static void render_disenio_mapa(void) {
    consola_limpiar();
    puts("+================================================+");
    puts("|                 DISENAR MAPA                   |");
    puts("+================================================+");
    puts("|  Ingresa las 10 filas del mapa.                |");
    puts("|  Usa '#' para paredes y ' ' para caminos.      |");
    puts("|  Cada fila debe tener exactamente 20 chars.    |");
    puts("+================================================+\n");
}

static void render_mensaje(const char *titulo, const char *msg) {
    consola_limpiar();
    puts("+================================+");
    printf("|  %-30s  |\n", titulo);
    puts("+================================+");
    printf("|  %-30s  |\n", msg);
    puts("+================================+");
    puts("\n  [cualquier tecla] Continuar...");
    fflush(stdout);
}

/* ---------------------------------------------
   DISENAR MAPA
   --------------------------------------------- */

static void diseniar_mapa(Juego *j) {
    render_disenio_mapa();
    char linea[64];
    int filas_ok = 0;
    for (int y = 0; y < ALTO; ) {
        printf("  Fila %2d/%d: ", y + 1, ALTO);
        fflush(stdout);
        if (!fgets(linea, sizeof(linea), stdin)) break;
        int len = strlen(linea);
        if (len > 0 && linea[len-1] == '\n') linea[--len] = '\0';
        if (len != ANCHO) {
            printf("  ERROR: debe tener %d chars (tiene %d). Intenta de nuevo.\n", ANCHO, len);
            continue;
        }
        strncpy(MAPA_CUSTOM[y], linea, ANCHO);
        MAPA_CUSTOM[y][ANCHO] = '\0';
        y++; filas_ok++;
    }
    if (filas_ok == ALTO) { usar_mapa_custom = 1; printf("\n  OK: Mapa guardado.\n"); }
    else                  { printf("\n  ERROR: Mapa incompleto, se mantiene el anterior.\n"); }
    printf("  [ENTER] Continuar...");
    fflush(stdout);
    fgets(linea, sizeof(linea), stdin);
    (void)j;
}

/* ---------------------------------------------
   MAQUINA DE ESTADOS
   --------------------------------------------- */

static void procesar_estado_menu(Juego *j) {
    render_menu(j);
    char c = consola_leer_tecla();
    switch (c) {
        case '1':
            if (j->hay_guardado) j->estado = ESTADO_CONFIRMAR_NUEVA;
            else { juego_inicializar(j); j->estado = ESTADO_JUGANDO; }
            break;
        case '2':
            if (j->partida_activa) j->estado = ESTADO_JUGANDO;
            break;
        case '3':
            if (j->hay_guardado) {
                if (partida_cargar(j)) j->estado = ESTADO_JUGANDO;
                else {
                    j->hay_guardado = 0;
                    render_mensaje("ERROR", "Archivo de guardado corrupto.");
                    consola_leer_tecla();
                }
            }
            break;
        case '4': j->estado = ESTADO_INSTRUCCIONES; break;
        case '5': j->estado = ESTADO_DISENIO_MAPA;  break;
        case 'q': case 'Q': j->estado = ESTADO_SALIR; break;
        default: break;
    }
}

static void procesar_estado_jugando(Juego *j) {
    render_juego(j);
    char c = consola_leer_tecla();
    j->ultima_tecla = c;
    int dx = 0, dy = 0, mov = 1;

    switch (c) {
        case 'w': case 'W': dy = -1; break;
        case 's': case 'S': dy =  1; break;
        case 'a': case 'A': dx = -1; break;
        case 'd': case 'D': dx =  1; break;
        case 'g': case 'G': {
            int ok = partida_guardar(j);
            j->hay_guardado = ok;
            render_mensaje(ok ? "GUARDADO OK" : "ERROR AL GUARDAR",
                           ok ? "Guardado en partida.dat" : "No se pudo guardar.");
            consola_leer_tecla();
            mov = 0; break;
        }
        case 'q': case 'Q': j->estado = ESTADO_CONFIRMAR_SALIDA; mov = 0; break;
        case 'i': case 'I': j->estado = ESTADO_INSTRUCCIONES;    mov = 0; break;
        default:
            snprintf(j->ultimo_mensaje, sizeof(j->ultimo_mensaje),
                     "Comando no valido: [%c]", (c >= 32 ? c : '?'));
            mov = 0; break;
    }
    if (mov && (dx || dy)) juego_intentar_mover(j, dx, dy);
}

static void procesar_estado_victoria(Juego *j) {
    render_victoria(j);
    consola_leer_tecla();
    /* Borrar guardado al ganar para que empiece limpio */
    remove(ARCHIVO_GUARDADO);
    j->hay_guardado   = 0;
    j->partida_activa = 0;
    j->estado         = ESTADO_MENU;
}

static void procesar_estado_instrucciones(Juego *j) {
    render_instrucciones();
    consola_leer_tecla();
    j->estado = j->partida_activa ? ESTADO_JUGANDO : ESTADO_MENU;
}

static void procesar_estado_disenio(Juego *j) {
    diseniar_mapa(j);
    j->estado = ESTADO_MENU;
}

static void procesar_estado_confirmar(Juego *j) {
    render_confirmar_salida();
    char c = consola_leer_tecla();
    if (c == 'g' || c == 'G') {
        int ok = partida_guardar(j);
        j->hay_guardado = ok;
        render_mensaje(ok ? "GUARDADO OK" : "ERROR AL GUARDAR",
                       ok ? "Volviendo al menu..." : "Error al guardar.");
        consola_leer_tecla();
        j->partida_activa = 0; j->estado = ESTADO_MENU;
    } else if (c == 's' || c == 'S') {
        j->partida_activa = 0; j->estado = ESTADO_MENU;
    } else {
        j->estado = ESTADO_JUGANDO;
        strncpy(j->ultimo_mensaje, "Salida cancelada.", sizeof(j->ultimo_mensaje) - 1);
    }
}

static void procesar_estado_confirmar_nueva(Juego *j) {
    render_confirmar_nueva();
    char c = consola_leer_tecla();
    if (c == 's' || c == 'S') { juego_inicializar(j); j->estado = ESTADO_JUGANDO; }
    else                       { j->estado = ESTADO_MENU; }
}

/* ---------------------------------------------
   MAIN
   --------------------------------------------- */

int main(void) {
    srand((unsigned int)time(NULL));  /* semilla aleatoria */

    Juego juego;
    memset(&juego, 0, sizeof(juego));
    juego.estado         = ESTADO_MENU;
    juego.partida_activa = 0;
    juego.hay_guardado   = partida_existe_guardado();

    hc_global = GetStdHandle(STD_OUTPUT_HANDLE);
    consola_modo_raw();

    while (juego.estado != ESTADO_SALIR) {
        switch (juego.estado) {
            case ESTADO_MENU:             procesar_estado_menu(&juego);            break;
            case ESTADO_JUGANDO:          procesar_estado_jugando(&juego);         break;
            case ESTADO_VICTORIA:         procesar_estado_victoria(&juego);        break;
            case ESTADO_INSTRUCCIONES:    procesar_estado_instrucciones(&juego);   break;
            case ESTADO_DISENIO_MAPA:     procesar_estado_disenio(&juego);         break;
            case ESTADO_CONFIRMAR_SALIDA: procesar_estado_confirmar(&juego);       break;
            case ESTADO_CONFIRMAR_NUEVA:  procesar_estado_confirmar_nueva(&juego); break;
            default: juego.estado = ESTADO_SALIR; break;
        }
    }

    consola_modo_normal();
    consola_limpiar();
    puts("!Hasta la proxima, explorador!");
    return 0;
}
