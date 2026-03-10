/*
 * juego_completo.c
 * Juego de exploracion — maquina de estados, trofeos, enemigos, vidas.
 * Renderizado con doble buffer (WriteConsoleOutputW) para eliminar parpadeo.
 *
 * Compilar : gcc juego_completo.c -o juego
 * Ejecutar : ./juego
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <conio.h>
#include <windows.h>

/* =============================================
   CONSTANTES
   ============================================= */

#define ALTO               10
#define ANCHO              20
#define ARCHIVO_GUARDADO   "partida.dat"
#define MAGIC_NUMBER       0xDADD

#define VIDAS_INICIALES    3
#define TROFEOS_PARA_GANAR 20
#define TROFEOS_POR_VIDA   5
#define SIMBOLO_TROFEO     '*'
#define SIMBOLO_ENEMIGO    'E'

#define MAX_ENEMIGOS       3
#define FRAMES_POR_TURNO   8
#define DELAY_FRAME_MS     80

/* Ancho del buffer de pantalla (columnas que ocupa el render del juego) */
#define BUF_COLS  50
#define BUF_ROWS  22   /* HUD(5) + mapa(10) + controles(2) + margen */

/* =============================================
   DOBLE BUFFER
   ============================================= */

/* Cada celda del buffer guarda un caracter y su atributo de color */
typedef struct { WCHAR ch; WORD attr; } Celda;

static Celda  s_buf[BUF_ROWS][BUF_COLS];
static int    s_cur_row = 0;
static int    s_cur_col = 0;
static WORD   s_cur_attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

/* Color "normal" (blanco) */
#define ATTR_NORMAL   (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define ATTR_GRIS     (FOREGROUND_INTENSITY)
#define ATTR_AMARILLO (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define ATTR_CIAN     (FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define ATTR_ROJO     (FOREGROUND_RED | FOREGROUND_INTENSITY)
#define ATTR_VERDE    (FOREGROUND_GREEN | FOREGROUND_INTENSITY)

static void buf_limpiar(void) {
    for (int r = 0; r < BUF_ROWS; r++)
        for (int c = 0; c < BUF_COLS; c++) {
            s_buf[r][c].ch   = L' ';
            s_buf[r][c].attr = ATTR_NORMAL;
        }
    s_cur_row = 0; s_cur_col = 0;
    s_cur_attr = ATTR_NORMAL;
}

static void buf_set_attr(WORD a) { s_cur_attr = a; }

/* Escribe un caracter en la posicion actual y avanza */
static void buf_putc(char c) {
    if (c == '\n') {
        /* rellenar el resto de la fila con espacios */
        while (s_cur_col < BUF_COLS) {
            if (s_cur_row < BUF_ROWS) {
                s_buf[s_cur_row][s_cur_col].ch   = L' ';
                s_buf[s_cur_row][s_cur_col].attr = ATTR_NORMAL;
            }
            s_cur_col++;
        }
        s_cur_row++;
        s_cur_col = 0;
        return;
    }
    if (s_cur_row >= BUF_ROWS || s_cur_col >= BUF_COLS) return;
    s_buf[s_cur_row][s_cur_col].ch   = (WCHAR)(unsigned char)c;
    s_buf[s_cur_row][s_cur_col].attr = s_cur_attr;
    s_cur_col++;
}

/* Escribe una cadena */
static void buf_puts(const char *s) {
    while (*s) buf_putc(*s++);
    buf_putc('\n');
}

/* Escribe con formato (usa snprintf internamente) */
static void buf_printf(const char *fmt, ...) {
    char tmp[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    for (int i = 0; tmp[i]; i++) buf_putc(tmp[i]);
}

/* Vuelca el buffer a la consola de una sola vez (sin parpadeo) */
static void buf_flush(void) {
    static HANDLE hOut = NULL;
    if (!hOut) hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    /* Ocultar cursor para evitar parpadeo residual */
    CONSOLE_CURSOR_INFO ci = {1, FALSE};
    SetConsoleCursorInfo(hOut, &ci);

    /* Convertir el buffer a CHAR_INFO */
    CHAR_INFO ci_buf[BUF_ROWS][BUF_COLS];
    for (int r = 0; r < BUF_ROWS; r++)
        for (int c = 0; c < BUF_COLS; c++) {
            ci_buf[r][c].Char.AsciiChar = (char)s_buf[r][c].ch;
            ci_buf[r][c].Attributes     = s_buf[r][c].attr;
        }

    SMALL_RECT region = {0, 0, BUF_COLS - 1, BUF_ROWS - 1};
    COORD buf_size    = {BUF_COLS, BUF_ROWS};
    COORD buf_origin  = {0, 0};

    WriteConsoleOutputA(hOut, (CHAR_INFO *)ci_buf, buf_size, buf_origin, &region);

    /* Mover cursor debajo del area renderizada para no interferir */
    COORD pos = {0, BUF_ROWS};
    SetConsoleCursorPosition(hOut, pos);
}

/* =============================================
   ESTADOS
   ============================================= */

typedef enum {
    ESTADO_MENU,
    ESTADO_JUGANDO,
    ESTADO_GAME_OVER,
    ESTADO_VICTORIA,
    ESTADO_INSTRUCCIONES,
    ESTADO_DISENIO_MAPA,
    ESTADO_CONFIRMAR_SALIDA,
    ESTADO_CONFIRMAR_NUEVA,
    ESTADO_SALIR
} EstadoJuego;

/* =============================================
   ESTRUCTURAS
   ============================================= */

typedef struct { int x, y, activo; } Enemigo;

typedef struct {
    char        mapa[ALTO][ANCHO + 1];
    int         jugador_x, jugador_y;
    int         spawn_x,   spawn_y;
    int         trofeo_x,  trofeo_y;
    int         trofeos_recogidos;
    int         trofeos_acum;
    int         vidas;
    int         pasos;
    int         choques;
    int         frame_contador;
    Enemigo     enemigos[MAX_ENEMIGOS];
    char        ultima_tecla;
    char        ultimo_mensaje[64];
    EstadoJuego estado;
    int         partida_activa;
    int         hay_guardado;
} Juego;

typedef struct {
    unsigned int magic;
    char    mapa[ALTO][ANCHO + 1];
    int     jugador_x, jugador_y;
    int     spawn_x,   spawn_y;
    int     trofeo_x,  trofeo_y;
    int     trofeos_recogidos, trofeos_acum;
    int     vidas, pasos, choques;
    Enemigo enemigos[MAX_ENEMIGOS];
} DatosGuardado;

/* =============================================
   MAPA BASE
   ============================================= */

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

/* =============================================
   CONSOLA HELPERS
   ============================================= */

static void consola_modo_raw(void)    {}
static void consola_modo_normal(void) {}

/* Limpia la pantalla real solo al entrar a menus estaticos */
static void consola_limpiar_real(void) { system("cls"); }

static char consola_leer_tecla(void) {
    int c = _getch();
    if (c == 0 || c == 0xE0) { _getch(); return 0; }
    return (char)c;
}
static char consola_leer_tecla_nb(void) {
    if (!_kbhit()) return 0;
    return consola_leer_tecla();
}

/* =============================================
   GUARDADO / CARGA
   ============================================= */

static int partida_guardar(const Juego *j) {
    FILE *f = fopen(ARCHIVO_GUARDADO, "wb");
    if (!f) return 0;
    DatosGuardado d;
    memset(&d, 0, sizeof(d));
    d.magic             = MAGIC_NUMBER;
    d.jugador_x         = j->jugador_x; d.jugador_y         = j->jugador_y;
    d.spawn_x           = j->spawn_x;   d.spawn_y           = j->spawn_y;
    d.trofeo_x          = j->trofeo_x;  d.trofeo_y          = j->trofeo_y;
    d.trofeos_recogidos = j->trofeos_recogidos;
    d.trofeos_acum      = j->trofeos_acum;
    d.vidas             = j->vidas;
    d.pasos             = j->pasos;
    d.choques           = j->choques;
    for (int y = 0; y < ALTO; y++) strncpy(d.mapa[y], j->mapa[y], ANCHO + 1);
    for (int i = 0; i < MAX_ENEMIGOS; i++) d.enemigos[i] = j->enemigos[i];
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
    j->jugador_x         = d.jugador_x; j->jugador_y         = d.jugador_y;
    j->spawn_x           = d.spawn_x;   j->spawn_y           = d.spawn_y;
    j->trofeo_x          = d.trofeo_x;  j->trofeo_y          = d.trofeo_y;
    j->trofeos_recogidos = d.trofeos_recogidos;
    j->trofeos_acum      = d.trofeos_acum;
    j->vidas             = d.vidas;
    j->pasos             = d.pasos;
    j->choques           = d.choques;
    for (int y = 0; y < ALTO; y++) strncpy(j->mapa[y], d.mapa[y], ANCHO + 1);
    for (int i = 0; i < MAX_ENEMIGOS; i++) j->enemigos[i] = d.enemigos[i];
    j->partida_activa = 1; j->ultima_tecla = ' '; j->frame_contador = 0;
    strncpy(j->ultimo_mensaje, "Partida cargada!", sizeof(j->ultimo_mensaje)-1);
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

/* =============================================
   LOGICA AUXILIAR
   ============================================= */

static int juego_es_pared(const Juego *j, int x, int y) {
    if (x < 0 || x >= ANCHO || y < 0 || y >= ALTO) return 1;
    return (j->mapa[y][x] == '#');
}

static int celda_tiene_enemigo(const Juego *j, int x, int y) {
    for (int i = 0; i < MAX_ENEMIGOS; i++)
        if (j->enemigos[i].activo && j->enemigos[i].x == x && j->enemigos[i].y == y)
            return 1;
    return 0;
}

static int celda_libre_aleatoria(const Juego *j, int *ox, int *oy) {
    int xs[ALTO*ANCHO], ys[ALTO*ANCHO], n = 0;
    for (int y = 0; y < ALTO; y++)
        for (int x = 0; x < ANCHO; x++) {
            if (juego_es_pared(j, x, y)) continue;
            if (x == j->jugador_x && y == j->jugador_y) continue;
            if (x == j->trofeo_x  && y == j->trofeo_y)  continue;
            if (celda_tiene_enemigo(j, x, y)) continue;
            xs[n] = x; ys[n] = y; n++;
        }
    if (!n) return 0;
    int idx = rand() % n; *ox = xs[idx]; *oy = ys[idx];
    return 1;
}

static void trofeo_colocar(Juego *j) {
    int x, y;
    if (celda_libre_aleatoria(j, &x, &y)) { j->trofeo_x = x; j->trofeo_y = y; }
}

static void enemigos_inicializar(Juego *j) {
    for (int i = 0; i < MAX_ENEMIGOS; i++) {
        int x, y;
        if (celda_libre_aleatoria(j, &x, &y)) {
            j->enemigos[i].x = x; j->enemigos[i].y = y; j->enemigos[i].activo = 1;
        } else j->enemigos[i].activo = 0;
    }
}

static void enemigo_mover_uno(Juego *j, int idx) {
    Enemigo *e = &j->enemigos[idx];
    if (!e->activo) return;
    int dirs[4][2] = {{0,-1},{0,1},{-1,0},{1,0}};
    for (int k = 3; k > 0; k--) {
        int r = rand() % (k+1);
        int tx = dirs[k][0], ty = dirs[k][1];
        dirs[k][0] = dirs[r][0]; dirs[k][1] = dirs[r][1];
        dirs[r][0] = tx;         dirs[r][1] = ty;
    }
    for (int k = 0; k < 4; k++) {
        int nx = e->x + dirs[k][0], ny = e->y + dirs[k][1];
        if (juego_es_pared(j, nx, ny) || celda_tiene_enemigo(j, nx, ny)) continue;
        e->x = nx; e->y = ny; return;
    }
}

static void enemigos_mover(Juego *j) {
    for (int i = 0; i < MAX_ENEMIGOS; i++) enemigo_mover_uno(j, i);
}

static int enemigos_colision_jugador(const Juego *j) {
    for (int i = 0; i < MAX_ENEMIGOS; i++)
        if (j->enemigos[i].activo &&
            j->enemigos[i].x == j->jugador_x &&
            j->enemigos[i].y == j->jugador_y) return 1;
    return 0;
}

static void aplicar_colision_enemigo(Juego *j) {
    j->vidas--;
    if (j->trofeos_recogidos > 0) j->trofeos_recogidos--;
    if (j->trofeos_acum      > 0) j->trofeos_acum--;
    j->jugador_x = j->spawn_x;
    j->jugador_y = j->spawn_y;
    if (j->vidas <= 0) { j->vidas = 0; j->estado = ESTADO_GAME_OVER; return; }
    snprintf(j->ultimo_mensaje, sizeof(j->ultimo_mensaje),
             "!Golpe! Vidas:%d  Trofeos:%d", j->vidas, j->trofeos_recogidos);
}

/* =============================================
   INICIALIZACION
   ============================================= */

static void juego_inicializar(Juego *j) {
    const char **fuente = usar_mapa_custom ? (const char **)MAPA_CUSTOM : MAPA_FIJO;
    for (int y = 0; y < ALTO; y++) {
        strncpy(j->mapa[y], fuente[y], ANCHO);
        j->mapa[y][ANCHO] = '\0';
    }
    j->jugador_x = 1; j->jugador_y = 1;
    if (juego_es_pared(j, 1, 1))
        for (int y = 1; y < ALTO-1; y++)
            for (int x = 1; x < ANCHO-1; x++)
                if (!juego_es_pared(j, x, y)) {
                    j->jugador_x = x; j->jugador_y = y; y = ALTO; break;
                }
    j->spawn_x = j->jugador_x; j->spawn_y = j->jugador_y;
    j->vidas = VIDAS_INICIALES; j->pasos = 0; j->choques = 0;
    j->trofeos_recogidos = 0; j->trofeos_acum = 0;
    j->ultima_tecla = ' '; j->partida_activa = 1; j->frame_contador = 0;
    trofeo_colocar(j);
    enemigos_inicializar(j);
    strncpy(j->ultimo_mensaje, "!Esquiva los enemigos!", sizeof(j->ultimo_mensaje)-1);
}

static void juego_intentar_mover(Juego *j, int dx, int dy) {
    int nx = j->jugador_x + dx, ny = j->jugador_y + dy;
    if (juego_es_pared(j, nx, ny)) {
        j->choques++;
        snprintf(j->ultimo_mensaje, sizeof(j->ultimo_mensaje), "!Choque! Total:%d", j->choques);
        return;
    }
    j->jugador_x = nx; j->jugador_y = ny; j->pasos++;

    if (nx == j->trofeo_x && ny == j->trofeo_y) {
        j->trofeos_recogidos++;
        j->trofeos_acum++;
        if (j->trofeos_acum >= TROFEOS_POR_VIDA) {
            j->trofeos_acum -= TROFEOS_POR_VIDA;
            j->vidas++;
            snprintf(j->ultimo_mensaje, sizeof(j->ultimo_mensaje),
                     "!+1 Vida! Vidas:%d  Trofeos:%d/%d",
                     j->vidas, j->trofeos_recogidos, TROFEOS_PARA_GANAR);
        } else {
            snprintf(j->ultimo_mensaje, sizeof(j->ultimo_mensaje),
                     "!Trofeo! %d/%d  (vida en %d)",
                     j->trofeos_recogidos, TROFEOS_PARA_GANAR,
                     TROFEOS_POR_VIDA - j->trofeos_acum);
        }
        if (j->trofeos_recogidos >= TROFEOS_PARA_GANAR) {
            j->trofeo_x = -1; j->trofeo_y = -1;
            j->estado = ESTADO_VICTORIA; return;
        }
        trofeo_colocar(j);
    } else {
        snprintf(j->ultimo_mensaje, sizeof(j->ultimo_mensaje),
                 "Trofeos:%d/%d  Vida extra en %d",
                 j->trofeos_recogidos, TROFEOS_PARA_GANAR,
                 TROFEOS_POR_VIDA - j->trofeos_acum);
    }
}

/* =============================================
   RENDERS (usan buffer — sin parpadeo)
   ============================================= */

static void render_juego(const Juego *j) {
    buf_limpiar();

    /* -- HUD linea 1 -- */
    buf_set_attr(ATTR_NORMAL);
    buf_puts("+------------------------------------------+");
    buf_printf("| Pasos:%-5d Choques:%-4d Pos:(%2d,%2d)      |\n",
               j->pasos, j->choques, j->jugador_x, j->jugador_y);

    /* -- Barra trofeos -- */
    buf_printf("| Trofeos:[");
    for (int i = 0; i < TROFEOS_PARA_GANAR; i++) {
        if (i > 0 && i % 5 == 0) {
            buf_set_attr(ATTR_GRIS); buf_putc('|');
        }
        if (i < j->trofeos_recogidos) {
            buf_set_attr(ATTR_AMARILLO); buf_putc('*');
        } else {
            buf_set_attr(ATTR_GRIS);    buf_putc('.');
        }
    }
    buf_set_attr(ATTR_NORMAL);
    buf_printf("] %2d/%-2d |\n", j->trofeos_recogidos, TROFEOS_PARA_GANAR);

    /* -- Vidas -- */
    buf_printf("| Vidas: ");
    for (int i = 0; i < j->vidas && i < 9; i++) {
        buf_set_attr(ATTR_ROJO); buf_putc('v');
    }
    buf_set_attr(ATTR_NORMAL);
    for (int i = j->vidas; i < 9; i++) buf_putc(' ');
    buf_printf(" Vida extra en %d trofeo(s) |\n", TROFEOS_POR_VIDA - j->trofeos_acum);

    /* -- Mensaje -- */
    buf_set_attr(ATTR_NORMAL);
    buf_printf("| %-42s |\n", j->ultimo_mensaje);
    buf_puts("+------------------------------------------+");

    /* -- Mapa -- */
    for (int y = 0; y < ALTO; y++) {
        buf_set_attr(ATTR_NORMAL);
        buf_putc(' ');
        for (int x = 0; x < ANCHO; x++) {
            if (x == j->jugador_x && y == j->jugador_y) {
                buf_set_attr(ATTR_AMARILLO); buf_putc('@');
            } else if (celda_tiene_enemigo(j, x, y)) {
                buf_set_attr(ATTR_ROJO);     buf_putc(SIMBOLO_ENEMIGO);
            } else if (x == j->trofeo_x && y == j->trofeo_y) {
                buf_set_attr(ATTR_CIAN);     buf_putc(SIMBOLO_TROFEO);
            } else if (j->mapa[y][x] == '#') {
                buf_set_attr(ATTR_GRIS);     buf_putc('#');
            } else {
                buf_set_attr(ATTR_NORMAL);   buf_putc(j->mapa[y][x]);
            }
        }
        buf_putc('\n');
    }

    buf_set_attr(ATTR_NORMAL);
    buf_puts(" [w/a/s/d] Mover | [g] Guardar | [q] Menu | [i] Info");

    buf_flush();
}

/* Los menus usan consola normal (se muestran una vez, no necesitan anti-parpadeo) */

static void render_menu(const Juego *j) {
    consola_limpiar_real();
    puts("+================================+");
    puts("|     EXPLORADOR DE MAZMORRA     |");
    puts("|   Recolecta 20 trofeos!        |");
    puts("+================================+");
    puts("|  [1] Nueva partida             |");
    puts(j->partida_activa
         ? "|  [2] Continuar partida         |"
         : "|  [2] Continuar  (sin partida)  |");
    puts(j->hay_guardado
         ? "|  [3] Cargar partida guardada   |"
         : "|  [3] Cargar  (sin guardado)    |");
    puts("|  [4] Instrucciones             |");
    puts("|  [5] Disenar Mapa              |");
    puts("|  [q] Salir                     |");
    puts("+================================+");
    printf("\n  Elige una opcion: ");
    fflush(stdout);
}

static void render_game_over(const Juego *j) {
    consola_limpiar_real();
    puts("+========================================+");
    puts("|              GAME  OVER                |");
    puts("+========================================+");
    printf("|  Trofeos recogidos: %-5d              |\n", j->trofeos_recogidos);
    printf("|  Pasos totales    : %-5d              |\n", j->pasos);
    printf("|  Choques totales  : %-5d              |\n", j->choques);
    puts("+========================================+");
    puts("\n  [cualquier tecla] Volver al menu...");
    fflush(stdout);
}

static void render_victoria(const Juego *j) {
    consola_limpiar_real();
    puts("+========================================+");
    puts("|       !!! FELICITACIONES !!!           |");
    puts("|   Has recolectado los 20 trofeos!      |");
    puts("+========================================+");
    printf("|  Pasos totales  : %-5d                |\n", j->pasos);
    printf("|  Choques totales: %-5d                |\n", j->choques);
    printf("|  Vidas restantes: %-5d                |\n", j->vidas);
    puts("+========================================+");
    puts("\n  [cualquier tecla] Volver al menu...");
    fflush(stdout);
}

static void render_instrucciones(void) {
    consola_limpiar_real();
    puts("+==========================================+");
    puts("|              INSTRUCCIONES               |");
    puts("+==========================================+");
    puts("|  w/a/s/d -> Mover al jugador             |");
    puts("|  g / G   -> Guardar partida              |");
    puts("|  q / Q   -> Volver al menu               |");
    puts("|  i / I   -> Ver instrucciones            |");
    puts("+==========================================+");
    puts("|  @  = tu personaje (amarillo)            |");
    puts("|  *  = trofeo a recoger (cian)            |");
    puts("|  E  = enemigo (rojo) -- se mueve solo!   |");
    puts("|  #  = pared                              |");
    puts("+==========================================+");
    puts("|  Objetivo: recolectar 20 trofeos.        |");
    puts("|  Colision con E: -1 vida, -1 trofeo.     |");
    puts("|  Cada 5 trofeos: +1 vida.                |");
    puts("|  Sin vidas: Game Over.                   |");
    puts("+==========================================+");
    puts("\n  [cualquier tecla] Volver...");
    fflush(stdout);
}

static void render_confirmar_salida(void) {
    consola_limpiar_real();
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
    consola_limpiar_real();
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
    consola_limpiar_real();
    puts("+================================================+");
    puts("|                 DISENAR MAPA                   |");
    puts("+================================================+");
    puts("|  Ingresa las 10 filas del mapa.                |");
    puts("|  Usa '#' para paredes y ' ' para caminos.      |");
    puts("|  Cada fila debe tener exactamente 20 chars.    |");
    puts("+================================================+\n");
}

static void render_mensaje(const char *titulo, const char *msg) {
    consola_limpiar_real();
    puts("+================================+");
    printf("|  %-30s  |\n", titulo);
    puts("+================================+");
    printf("|  %-30s  |\n", msg);
    puts("+================================+");
    puts("\n  [cualquier tecla] Continuar...");
    fflush(stdout);
}

/* =============================================
   DISENAR MAPA
   ============================================= */

static void diseniar_mapa(Juego *j) {
    render_disenio_mapa();
    char linea[64]; int filas_ok = 0;
    for (int y = 0; y < ALTO; ) {
        printf("  Fila %2d/%d: ", y+1, ALTO); fflush(stdout);
        if (!fgets(linea, sizeof(linea), stdin)) break;
        int len = strlen(linea);
        if (len > 0 && linea[len-1] == '\n') linea[--len] = '\0';
        if (len != ANCHO) { printf("  ERROR: debe tener %d chars.\n", ANCHO); continue; }
        strncpy(MAPA_CUSTOM[y], linea, ANCHO);
        MAPA_CUSTOM[y][ANCHO] = '\0'; y++; filas_ok++;
    }
    if (filas_ok == ALTO) { usar_mapa_custom = 1; printf("\n  OK: Mapa guardado.\n"); }
    else { printf("\n  ERROR: Mapa incompleto.\n"); }
    printf("  [ENTER] Continuar..."); fflush(stdout);
    fgets(linea, sizeof(linea), stdin);
    (void)j;
}

/* =============================================
   MAQUINA DE ESTADOS
   ============================================= */

static void procesar_estado_menu(Juego *j) {
    render_menu(j);
    char c = consola_leer_tecla();
    switch (c) {
        case '1':
            if (j->hay_guardado) j->estado = ESTADO_CONFIRMAR_NUEVA;
            else { juego_inicializar(j); j->estado = ESTADO_JUGANDO; }
            break;
        case '2': if (j->partida_activa) j->estado = ESTADO_JUGANDO; break;
        case '3':
            if (j->hay_guardado) {
                if (partida_cargar(j)) j->estado = ESTADO_JUGANDO;
                else {
                    j->hay_guardado = 0;
                    render_mensaje("ERROR", "Archivo corrupto."); consola_leer_tecla();
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

    /* Esperar input con timeout para que los enemigos avancen solos */
    int espera = DELAY_FRAME_MS;
    while (espera > 0 && !_kbhit()) { Sleep(10); espera -= 10; }

    char c = consola_leer_tecla_nb();
    if (c != 0) {
        j->ultima_tecla = c;
        int dx = 0, dy = 0, mov = 1;
        switch (c) {
            case 'w': case 'W': dy = -1; break;
            case 's': case 'S': dy =  1; break;
            case 'a': case 'A': dx = -1; break;
            case 'd': case 'D': dx =  1; break;
            case 'g': case 'G': {
                int ok = partida_guardar(j); j->hay_guardado = ok;
                render_mensaje(ok ? "GUARDADO OK" : "ERROR AL GUARDAR",
                               ok ? "Guardado en partida.dat" : "No se pudo guardar.");
                consola_leer_tecla(); mov = 0; break;
            }
            case 'q': case 'Q': j->estado = ESTADO_CONFIRMAR_SALIDA; mov = 0; break;
            case 'i': case 'I': j->estado = ESTADO_INSTRUCCIONES;    mov = 0; break;
            default:
                snprintf(j->ultimo_mensaje, sizeof(j->ultimo_mensaje),
                         "Comando no valido: [%c]", (c >= 32 ? c : '?'));
                mov = 0; break;
        }
        if (mov && (dx || dy)) juego_intentar_mover(j, dx, dy);
        if (j->estado != ESTADO_JUGANDO) return;
    }

    j->frame_contador++;
    if (j->frame_contador >= FRAMES_POR_TURNO) {
        j->frame_contador = 0;
        enemigos_mover(j);
    }

    if (enemigos_colision_jugador(j)) aplicar_colision_enemigo(j);
}

static void procesar_estado_game_over(Juego *j) {
    render_game_over(j); consola_leer_tecla();
    remove(ARCHIVO_GUARDADO);
    j->hay_guardado = 0; j->partida_activa = 0; j->estado = ESTADO_MENU;
}

static void procesar_estado_victoria(Juego *j) {
    render_victoria(j); consola_leer_tecla();
    remove(ARCHIVO_GUARDADO);
    j->hay_guardado = 0; j->partida_activa = 0; j->estado = ESTADO_MENU;
}

static void procesar_estado_instrucciones(Juego *j) {
    render_instrucciones(); consola_leer_tecla();
    j->estado = j->partida_activa ? ESTADO_JUGANDO : ESTADO_MENU;
}

static void procesar_estado_disenio(Juego *j) {
    diseniar_mapa(j); j->estado = ESTADO_MENU;
}

static void procesar_estado_confirmar(Juego *j) {
    render_confirmar_salida();
    char c = consola_leer_tecla();
    if (c == 'g' || c == 'G') {
        int ok = partida_guardar(j); j->hay_guardado = ok;
        render_mensaje(ok ? "GUARDADO OK" : "ERROR AL GUARDAR",
                       ok ? "Volviendo al menu..." : "Error al guardar.");
        consola_leer_tecla();
        j->partida_activa = 0; j->estado = ESTADO_MENU;
    } else if (c == 's' || c == 'S') {
        j->partida_activa = 0; j->estado = ESTADO_MENU;
    } else {
        j->estado = ESTADO_JUGANDO;
        strncpy(j->ultimo_mensaje, "Salida cancelada.", sizeof(j->ultimo_mensaje)-1);
    }
}

static void procesar_estado_confirmar_nueva(Juego *j) {
    render_confirmar_nueva();
    char c = consola_leer_tecla();
    if (c == 's' || c == 'S') { juego_inicializar(j); j->estado = ESTADO_JUGANDO; }
    else j->estado = ESTADO_MENU;
}

/* =============================================
   MAIN
   ============================================= */

int main(void) {
    srand((unsigned int)time(NULL));

    /* Agrandar la ventana de consola para que quepa el buffer sin scroll */
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    SMALL_RECT wr = {0, 0, BUF_COLS - 1, BUF_ROWS + 2};
    COORD      cs = {BUF_COLS, BUF_ROWS + 4};
    SetConsoleWindowInfo(hOut, TRUE, &wr);
    SetConsoleScreenBufferSize(hOut, cs);

    Juego juego;
    memset(&juego, 0, sizeof(juego));
    juego.estado         = ESTADO_MENU;
    juego.partida_activa = 0;
    juego.hay_guardado   = partida_existe_guardado();

    consola_modo_raw();

    while (juego.estado != ESTADO_SALIR) {
        switch (juego.estado) {
            case ESTADO_MENU:             procesar_estado_menu(&juego);            break;
            case ESTADO_JUGANDO:          procesar_estado_jugando(&juego);         break;
            case ESTADO_GAME_OVER:        procesar_estado_game_over(&juego);       break;
            case ESTADO_VICTORIA:         procesar_estado_victoria(&juego);        break;
            case ESTADO_INSTRUCCIONES:    procesar_estado_instrucciones(&juego);   break;
            case ESTADO_DISENIO_MAPA:     procesar_estado_disenio(&juego);         break;
            case ESTADO_CONFIRMAR_SALIDA: procesar_estado_confirmar(&juego);       break;
            case ESTADO_CONFIRMAR_NUEVA:  procesar_estado_confirmar_nueva(&juego); break;
            default: juego.estado = ESTADO_SALIR; break;
        }
    }

    consola_modo_normal();
    consola_limpiar_real();
    puts("!Hasta la proxima, explorador!");
    return 0;
}
