/*
 * juego_completo.c
 * Explorador de Mazmorra — maquina de estados, doble buffer, enemigos inteligentes.
 *
 * Compilar : gcc juego_completo.c -o juego
 * Ejecutar : ./juego
 *
 * Nuevas reglas:
 *  - MAX 5 enemigos. Empieza con 1, spawn en esquina inferior derecha.
 *  - Enemigos persiguen al jugador (BFS).
 *  - Enemigos pueden capturar trofeos; al acumular 2, se reproducen.
 *  - Pausa in-line: el menu de pausa aparece en 1 linea debajo del mapa.
 *  - Cargar guardado: se dibuja el estado guardado y arranca en pausa.
 *  - Guardado/carga almacena posicion exacta + estado completo.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <conio.h>
#include <windows.h>
#include <stdarg.h>

/* =============================================
   CONSTANTES
   ============================================= */

#define ALTO               10
#define ANCHO              20
#define ARCHIVO_GUARDADO   "partida.dat"
#define MAGIC_NUMBER       0xDADE   /* nuevo magic — invalida guardados anteriores */

#define VIDAS_INICIALES        3
#define TROFEOS_PARA_GANAR     20
#define TROFEOS_POR_VIDA       5
#define SIMBOLO_TROFEO         '*'
#define SIMBOLO_ENEMIGO        'E'

#define MAX_ENEMIGOS           5    /* maximo de enemigos simultaneos (hardcoded) */
#define TROFEOS_PARA_REPRODUCIR 2   /* trofeos que necesita un enemigo para reproducirse */
#define FRAMES_POR_TURNO       8    /* frames entre cada movimiento de enemigos */
#define DELAY_FRAME_MS         80

/* Dimensiones del buffer de pantalla */
#define BUF_COLS  52
#define BUF_ROWS  24   /* HUD(6) + mapa(10) + pausa(1) + margen */

/* Posicion de spawn de enemigos: esquina inferior derecha libre */
#define ENEMIGO_SPAWN_X  (ANCHO - 2)
#define ENEMIGO_SPAWN_Y  (ALTO  - 2)

/* =============================================
   DOBLE BUFFER
   ============================================= */

typedef struct { char ch; WORD attr; } Celda;

static Celda s_buf[BUF_ROWS][BUF_COLS];
static int   s_row = 0, s_col = 0;
static WORD  s_attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

#define ATTR_NORMAL   (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define ATTR_GRIS     (FOREGROUND_INTENSITY)
#define ATTR_AMARILLO (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define ATTR_CIAN     (FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define ATTR_ROJO     (FOREGROUND_RED | FOREGROUND_INTENSITY)
#define ATTR_MAGENTA  (FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define ATTR_VERDE    (FOREGROUND_GREEN | FOREGROUND_INTENSITY)

static void buf_limpiar(void) {
    for (int r = 0; r < BUF_ROWS; r++)
        for (int c = 0; c < BUF_COLS; c++)
            { s_buf[r][c].ch = ' '; s_buf[r][c].attr = ATTR_NORMAL; }
    s_row = 0; s_col = 0; s_attr = ATTR_NORMAL;
}

static void buf_attr(WORD a) { s_attr = a; }

static void buf_putc(char c) {
    if (c == '\n') {
        while (s_col < BUF_COLS && s_row < BUF_ROWS)
            { s_buf[s_row][s_col].ch = ' '; s_buf[s_row][s_col].attr = ATTR_NORMAL; s_col++; }
        s_row++; s_col = 0; return;
    }
    if (s_row >= BUF_ROWS || s_col >= BUF_COLS) return;
    s_buf[s_row][s_col].ch   = c;
    s_buf[s_row][s_col].attr = s_attr;
    s_col++;
}

static void buf_puts(const char *s) { while (*s) buf_putc(*s++); buf_putc('\n'); }

static void buf_printf(const char *fmt, ...) {
    char tmp[256]; va_list ap;
    va_start(ap, fmt); vsnprintf(tmp, sizeof(tmp), fmt, ap); va_end(ap);
    for (int i = 0; tmp[i]; i++) buf_putc(tmp[i]);
}

/* Mueve el cursor del buffer a una fila/columna especifica */
static void buf_goto(int row, int col) {
    s_row = row; s_col = col;
}

/* Escribe una linea horizontal en el buffer SIN avanzar de fila automaticamente
   (util para la linea de pausa in-line) */
static void buf_write_at(int row, int col, const char *s, WORD attr) {
    int saved_row = s_row, saved_col = s_col, saved_attr = s_attr;
    buf_goto(row, col); buf_attr(attr);
    while (*s && s_col < BUF_COLS) { buf_putc(*s++); }
    s_row = saved_row; s_col = saved_col; s_attr = saved_attr;
}

static void buf_flush(void) {
    static HANDLE hOut = NULL;
    if (!hOut) hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO ci = {1, FALSE};
    SetConsoleCursorInfo(hOut, &ci);

    CHAR_INFO ci_buf[BUF_ROWS][BUF_COLS];
    for (int r = 0; r < BUF_ROWS; r++)
        for (int c = 0; c < BUF_COLS; c++) {
            ci_buf[r][c].Char.AsciiChar = s_buf[r][c].ch;
            ci_buf[r][c].Attributes     = s_buf[r][c].attr;
        }
    SMALL_RECT region  = {0, 0, BUF_COLS-1, BUF_ROWS-1};
    COORD buf_size     = {BUF_COLS, BUF_ROWS};
    COORD buf_origin   = {0, 0};
    WriteConsoleOutputA(hOut, (CHAR_INFO*)ci_buf, buf_size, buf_origin, &region);

    COORD pos = {0, BUF_ROWS};
    SetConsoleCursorPosition(hOut, pos);
}

/* =============================================
   ESTADOS
   ============================================= */

typedef enum {
    ESTADO_MENU,
    ESTADO_JUGANDO,
    ESTADO_PAUSADO,       /* nuevo: pausa in-line */
    ESTADO_GAME_OVER,
    ESTADO_VICTORIA,
    ESTADO_INSTRUCCIONES,
    ESTADO_DISENIO_MAPA,
    ESTADO_CONFIRMAR_NUEVA,
    ESTADO_SALIR
} EstadoJuego;

/* =============================================
   ESTRUCTURAS
   ============================================= */

typedef struct {
    int  x, y;
    int  activo;
    int  trofeos_capturados; /* trofeos que este enemigo ha recogido */
} Enemigo;

typedef struct {
    char        mapa[ALTO][ANCHO + 1];
    int         jugador_x, jugador_y;
    int         spawn_x,   spawn_y;
    int         trofeo_x,  trofeo_y;
    int         trofeos_recogidos;   /* trofeos del jugador */
    int         trofeos_acum;        /* para calcular vida extra */
    int         vidas;
    int         pasos;
    int         choques;
    int         frame_contador;
    int         num_enemigos;        /* enemigos activos actualmente */
    Enemigo     enemigos[MAX_ENEMIGOS];
    char        ultima_tecla;
    char        ultimo_mensaje[64];
    EstadoJuego estado;
    int         partida_activa;
    int         hay_guardado;
} Juego;

/* Datos persistidos exactos */
typedef struct {
    unsigned int magic;
    char    mapa[ALTO][ANCHO + 1];
    int     jugador_x, jugador_y;
    int     spawn_x,   spawn_y;
    int     trofeo_x,  trofeo_y;
    int     trofeos_recogidos, trofeos_acum;
    int     vidas, pasos, choques;
    int     num_enemigos;
    Enemigo enemigos[MAX_ENEMIGOS];
    char    ultimo_mensaje[64];
    char    ultima_tecla;
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
   CONSOLA
   ============================================= */

static void consola_modo_raw(void)    {}
static void consola_modo_normal(void) {}
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
    d.jugador_x         = j->jugador_x;  d.jugador_y         = j->jugador_y;
    d.spawn_x           = j->spawn_x;    d.spawn_y           = j->spawn_y;
    d.trofeo_x          = j->trofeo_x;   d.trofeo_y          = j->trofeo_y;
    d.trofeos_recogidos = j->trofeos_recogidos;
    d.trofeos_acum      = j->trofeos_acum;
    d.vidas             = j->vidas;
    d.pasos             = j->pasos;
    d.choques           = j->choques;
    d.num_enemigos      = j->num_enemigos;
    d.ultima_tecla      = j->ultima_tecla;
    for (int y = 0; y < ALTO; y++) strncpy(d.mapa[y], j->mapa[y], ANCHO+1);
    for (int i = 0; i < MAX_ENEMIGOS; i++) d.enemigos[i] = j->enemigos[i];
    strncpy(d.ultimo_mensaje, j->ultimo_mensaje, sizeof(d.ultimo_mensaje)-1);
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
    j->jugador_x         = d.jugador_x;  j->jugador_y         = d.jugador_y;
    j->spawn_x           = d.spawn_x;    j->spawn_y           = d.spawn_y;
    j->trofeo_x          = d.trofeo_x;   j->trofeo_y          = d.trofeo_y;
    j->trofeos_recogidos = d.trofeos_recogidos;
    j->trofeos_acum      = d.trofeos_acum;
    j->vidas             = d.vidas;
    j->pasos             = d.pasos;
    j->choques           = d.choques;
    j->num_enemigos      = d.num_enemigos;
    j->ultima_tecla      = d.ultima_tecla;
    for (int y = 0; y < ALTO; y++) strncpy(j->mapa[y], d.mapa[y], ANCHO+1);
    for (int i = 0; i < MAX_ENEMIGOS; i++) j->enemigos[i] = d.enemigos[i];
    strncpy(j->ultimo_mensaje, d.ultimo_mensaje, sizeof(j->ultimo_mensaje)-1);
    j->partida_activa  = 1;
    j->frame_contador  = 0;
    /* Arranca en PAUSA para que el jugador vea el estado antes de reanudar */
    j->estado          = ESTADO_PAUSADO;
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

/* Coloca el trofeo en celda libre que no sea pared, jugador, ni enemigo */
static void trofeo_colocar(Juego *j) {
    int xs[ALTO*ANCHO], ys[ALTO*ANCHO], n = 0;
    for (int y = 0; y < ALTO; y++)
        for (int x = 0; x < ANCHO; x++) {
            if (juego_es_pared(j, x, y)) continue;
            if (x == j->jugador_x && y == j->jugador_y) continue;
            if (celda_tiene_enemigo(j, x, y)) continue;
            xs[n] = x; ys[n] = y; n++;
        }
    if (!n) return;
    int idx = rand() % n;
    j->trofeo_x = xs[idx]; j->trofeo_y = ys[idx];
}

/* =============================================
   BFS — PERSECUCION DE ENEMIGO
   Devuelve el primer paso optimo hacia (tx,ty) desde (sx,sy).
   Si no hay camino, devuelve (sx,sy) — el enemigo se queda quieto.
   ============================================= */

#define BFS_MAX (ALTO * ANCHO)

static void enemigo_bfs_paso(const Juego *j, int sx, int sy,
                              int tx, int ty,
                              int *nx, int *ny)
{
    /* Cola circular de celdas por visitar */
    int qx[BFS_MAX], qy[BFS_MAX], qhead = 0, qtail = 0;
    /* padre[y][x] codifica la celda de origen como y*ANCHO+x; -1 = sin visitar */
    int padre[ALTO][ANCHO];
    memset(padre, -1, sizeof(padre));

    padre[sy][sx] = sy * ANCHO + sx;   /* celda raiz apunta a si misma */
    // (eliminado duplicado)
    qx[qtail] = sx; qy[qtail] = sy; qtail++;

    int dirs[4][2] = {{0,-1},{0,1},{-1,0},{1,0}};
    int found = 0;

    while (qhead < qtail && !found) {
        int cx = qx[qhead], cy = qy[qhead]; qhead++;
        for (int d = 0; d < 4; d++) {
            int ex = cx + dirs[d][0], ey = cy + dirs[d][1];
            if (juego_es_pared(j, ex, ey)) continue;
            if (padre[ey][ex] != -1) continue;
            /* No caminar sobre otro enemigo (excepto el destino) */
            if (celda_tiene_enemigo(j, ex, ey) &&
                !(ex == tx && ey == ty)) continue;
            padre[ey][ex] = cy * ANCHO + cx;
            qx[qtail] = ex; qy[qtail] = ey; qtail++;
            if (ex == tx && ey == ty) { found = 1; break; }
        }
    }

    if (!found) { *nx = sx; *ny = sy; return; } /* sin camino */

    /* Reconstruir camino hacia atras hasta encontrar el primer paso */
    int cx = tx, cy = ty;
    while (1) {
        int pcod = padre[cy][cx];
        int px = pcod % ANCHO, py = pcod / ANCHO;
        if (px == sx && py == sy) { *nx = cx; *ny = cy; return; }
        cx = px; cy = py;
    }
}

/* =============================================
   MOVIMIENTO DE ENEMIGOS
   ============================================= */

static void enemigo_mover_uno(Juego *j, int idx) {
    Enemigo *e = &j->enemigos[idx];
    if (!e->activo) return;

    int nx, ny;
    enemigo_bfs_paso(j, e->x, e->y, j->jugador_x, j->jugador_y, &nx, &ny);

    /* Si la celda destino tiene otro enemigo, quedarse quieto este turno */
    if (celda_tiene_enemigo(j, nx, ny) && !(nx == e->x && ny == e->y)) return;

    e->x = nx; e->y = ny;

    /* Comprobar si el enemigo recoge el trofeo */
    if (e->x == j->trofeo_x && e->y == j->trofeo_y) {
        e->trofeos_capturados++;
        trofeo_colocar(j);   /* recolocar trofeo */

        /* Reproduccion: si llego a TROFEOS_PARA_REPRODUCIR y hay espacio */
        if (e->trofeos_capturados >= TROFEOS_PARA_REPRODUCIR &&
            j->num_enemigos < MAX_ENEMIGOS) {

            e->trofeos_capturados = 0; /* reiniciar contador del padre */

            /* Buscar slot libre */
            for (int i = 0; i < MAX_ENEMIGOS; i++) {
                if (!j->enemigos[i].activo) {
                    /* Nuevo enemigo en esquina inferior derecha */
                    j->enemigos[i].x                = ENEMIGO_SPAWN_X;
                    j->enemigos[i].y                = ENEMIGO_SPAWN_Y;
                    j->enemigos[i].activo           = 1;
                    j->enemigos[i].trofeos_capturados = 0;
                    j->num_enemigos++;
                    snprintf(j->ultimo_mensaje, sizeof(j->ultimo_mensaje),
                             "!Nuevo enemigo! Total: %d/%d",
                             j->num_enemigos, MAX_ENEMIGOS);
                    break;
                }
            }
        }
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
    j->jugador_x = j->spawn_x; j->jugador_y = j->spawn_y;
    if (j->vidas <= 0) { j->vidas = 0; j->estado = ESTADO_GAME_OVER; return; }
    snprintf(j->ultimo_mensaje, sizeof(j->ultimo_mensaje),
             "!Golpe! Vidas:%d  Trofeos:%d", j->vidas, j->trofeos_recogidos);
}

/* =============================================
   INICIALIZACION
   ============================================= */

static void juego_inicializar(Juego *j) {
    const char **fuente = usar_mapa_custom ? (const char**)MAPA_CUSTOM : MAPA_FIJO;
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

    /* Un solo enemigo inicial en la esquina inferior derecha */
    memset(j->enemigos, 0, sizeof(j->enemigos));
    j->enemigos[0].x      = ENEMIGO_SPAWN_X;
    j->enemigos[0].y      = ENEMIGO_SPAWN_Y;
    j->enemigos[0].activo = 1;
    j->enemigos[0].trofeos_capturados = 0;
    j->num_enemigos = 1;

    trofeo_colocar(j);
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
        j->trofeos_recogidos++; j->trofeos_acum++;
        if (j->trofeos_acum >= TROFEOS_POR_VIDA) {
            j->trofeos_acum -= TROFEOS_POR_VIDA; j->vidas++;
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
   RENDERS
   ============================================= */

/* Fila del buffer donde empieza el mapa (despues del HUD) */
#define HUD_ROWS 6
#define MAPA_ROW_START HUD_ROWS
/* Fila donde va la linea de pausa in-line */
#define PAUSA_ROW (MAPA_ROW_START + ALTO)

static void render_hud(const Juego *j) {
    buf_attr(ATTR_NORMAL);
    buf_puts("+--------------------------------------------------+");
    buf_printf("| Pasos:%-5d Choques:%-4d Pos:(%2d,%2d)             |\n",
               j->pasos, j->choques, j->jugador_x, j->jugador_y);

    /* Barra trofeos */
    buf_printf("| Trofeos:[");
    for (int i = 0; i < TROFEOS_PARA_GANAR; i++) {
        if (i > 0 && i % 5 == 0) { buf_attr(ATTR_GRIS); buf_putc('|'); }
        buf_attr(i < j->trofeos_recogidos ? ATTR_AMARILLO : ATTR_GRIS);
        buf_putc('*');
    }
    buf_attr(ATTR_NORMAL);
    buf_printf("] %2d/%-2d  |\n", j->trofeos_recogidos, TROFEOS_PARA_GANAR);

    /* Vidas */
    buf_printf("| Vidas: ");
    for (int i = 0; i < j->vidas && i < 9; i++) { buf_attr(ATTR_ROJO); buf_putc('v'); }
    buf_attr(ATTR_NORMAL);
    for (int i = j->vidas; i < 9; i++) buf_putc(' ');
    buf_printf("  Vida extra en %d  Enemigos:%d/%d |\n",
               TROFEOS_POR_VIDA - j->trofeos_acum,
               j->num_enemigos, MAX_ENEMIGOS);

    buf_attr(ATTR_NORMAL);
    buf_printf("| %-48s |\n", j->ultimo_mensaje);
    buf_puts("+--------------------------------------------------+");
}

static void render_mapa(const Juego *j) {
    for (int y = 0; y < ALTO; y++) {
        buf_attr(ATTR_NORMAL); buf_putc(' ');
        for (int x = 0; x < ANCHO; x++) {
            if (x == j->jugador_x && y == j->jugador_y) {
                buf_attr(ATTR_AMARILLO); buf_putc('@');
            } else if (celda_tiene_enemigo(j, x, y)) {
                buf_attr(ATTR_ROJO); buf_putc(SIMBOLO_ENEMIGO);
            } else if (x == j->trofeo_x && y == j->trofeo_y) {
                buf_attr(ATTR_CIAN); buf_putc(SIMBOLO_TROFEO);
            } else if (j->mapa[y][x] == '#') {
                buf_attr(ATTR_GRIS); buf_putc('#');
            } else {
                buf_attr(ATTR_NORMAL); buf_putc(j->mapa[y][x]);
            }
        }
        buf_putc('\n');
    }
}

/* Dibuja el juego completo (HUD + mapa) y luego la linea de controles */
static void render_juego_base(const Juego *j) {
    buf_limpiar();
    render_hud(j);
    render_mapa(j);
    /* Linea de controles normales */
    buf_attr(ATTR_GRIS);
    buf_puts(" [w/a/s/d] Mover | [g] Guardar | [p] Pausa | [i] Info");
    buf_attr(ATTR_NORMAL);
}

static void render_juego(const Juego *j) {
    render_juego_base(j);
    buf_flush();
}

/* Pausa IN-LINE: dibuja el juego y sobreescribe la linea de pausa */
static void render_pausa(const Juego *j) {
    render_juego_base(j);
    /* Sobreescribir la ultima linea (controles) con el menu de pausa */
    buf_write_at(PAUSA_ROW, 0,
        " PAUSA >> [r]Reanudar [g]Guardar [q]Menu principal [i]Info",
        ATTR_AMARILLO);
    buf_flush();
}

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
    puts("|  p / P   -> Pausar / Reanudar            |");
    puts("|  g / G   -> Guardar partida              |");
    puts("|  q / Q   -> Volver al menu               |");
    puts("|  i / I   -> Ver instrucciones            |");
    puts("+==========================================+");
    puts("|  @  = jugador (amarillo)                 |");
    puts("|  *  = trofeo  (cian)                     |");
    puts("|  E  = enemigo (rojo) — persigue y roba   |");
    puts("|  #  = pared                              |");
    puts("+==========================================+");
    puts("|  Objetivo: 20 trofeos antes de morir.    |");
    puts("|  Colision E: -1 vida, -1 trofeo.         |");
    puts("|  Cada 5 trofeos: +1 vida.                |");
    puts("|  Enemigo roba 2 trofeos -> se reproduce. |");
    puts("|  Max 5 enemigos; spawn en esq. inf. der. |");
    puts("+==========================================+");
    puts("\n  [cualquier tecla] Volver...");
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
                if (partida_cargar(j)) {
                    /* partida_cargar ya pone ESTADO_PAUSADO y dibuja sin limpiar */
                    render_pausa(j); /* mostrar estado cargado en pausa */
                } else {
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
            case 'p': case 'P':
                j->estado = ESTADO_PAUSADO; mov = 0; break;
            case 'g': case 'G': {
                int ok = partida_guardar(j); j->hay_guardado = ok;
                render_mensaje(ok ? "GUARDADO OK" : "ERROR AL GUARDAR",
                               ok ? "Guardado en partida.dat" : "No se pudo guardar.");
                consola_leer_tecla(); mov = 0; break;
            }
            case 'i': case 'I': j->estado = ESTADO_INSTRUCCIONES; mov = 0; break;
            case 'q': case 'Q':
                /* Pausa antes de preguntar si salir */
                j->estado = ESTADO_PAUSADO; mov = 0; break;
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

static void procesar_estado_pausado(Juego *j) {
    /* El mapa ya esta dibujado; solo refrescamos la linea de pausa */
    render_pausa(j);

    char c = consola_leer_tecla();   /* bloqueante: esperamos accion del jugador */
    switch (c) {
        case 'r': case 'R':
            j->estado = ESTADO_JUGANDO;
            strncpy(j->ultimo_mensaje, "Juego reanudado.", sizeof(j->ultimo_mensaje)-1);
            break;
        case 'g': case 'G': {
            int ok = partida_guardar(j); j->hay_guardado = ok;
            render_mensaje(ok ? "GUARDADO OK" : "ERROR AL GUARDAR",
                           ok ? "Guardado en partida.dat" : "No se pudo guardar.");
            consola_leer_tecla();
            /* Volvemos a pausa despues de guardar */
            break;
        }
        case 'q': case 'Q':
            /* Salir al menu sin preguntar (la pausa ya es una barrera de seguridad) */
            j->partida_activa = 0;
            j->estado = ESTADO_MENU;
            break;
        case 'i': case 'I':
            j->estado = ESTADO_INSTRUCCIONES;
            break;
        default:
            break;
    }
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
    /* Si habia partida activa, volvemos a pausa para que el jugador retome con calma */
    j->estado = j->partida_activa ? ESTADO_PAUSADO : ESTADO_MENU;
}

static void procesar_estado_disenio(Juego *j) {
    diseniar_mapa(j); j->estado = ESTADO_MENU;
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

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    SMALL_RECT wr = {0, 0, BUF_COLS-1, BUF_ROWS+2};
    COORD      cs = {BUF_COLS, BUF_ROWS+4};
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
            case ESTADO_PAUSADO:          procesar_estado_pausado(&juego);         break;
            case ESTADO_GAME_OVER:        procesar_estado_game_over(&juego);       break;
            case ESTADO_VICTORIA:         procesar_estado_victoria(&juego);        break;
            case ESTADO_INSTRUCCIONES:    procesar_estado_instrucciones(&juego);   break;
            case ESTADO_DISENIO_MAPA:     procesar_estado_disenio(&juego);         break;
            case ESTADO_CONFIRMAR_NUEVA:  procesar_estado_confirmar_nueva(&juego); break;
            default: juego.estado = ESTADO_SALIR; break;
        }
    }

    consola_modo_normal();
    consola_limpiar_real();
    puts("!Hasta la proxima, explorador!");
    return 0;
}
