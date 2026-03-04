/* Prototipo mejorado para desarrollo de videojuego
   - Mapa fijo en ASCII
   - Jugador '@'
   - Movimiento WASD
   - Colisiones con paredes
   - Sistema de monedas
*/

#include <stdio.h>
#include <string.h>

#define ALTO 10
#define ANCHO 20

/* 
'#' = pared
'.' = piso
'*' = moneda
*/

static const char *MAPA_FIJO[ALTO] = {
    "####################",
    "#.............*....#",
    "#.######.########..#",
    "#.#....#.#......#..#",
    "#.#.##.#.#.####.#..#",
    "#...##...#....#...*#",
    "#####.#########.####",
    "#.....#.......#....#",
    "#.#####.#####.###..#",
    "####################"
};

/* Limpieza simple de consola */
static void consola_limpiar_simple(void)
{
    for (int i = 0; i < 40; i++)
        printf("\n");
}

/* Verifica si una posición es pared */
static int es_pared(const char mapa[ALTO][ANCHO + 1], int x, int y)
{
    if (x < 0 || x >= ANCHO || y < 0 || y >= ALTO)
        return 1;

    return mapa[y][x] == '#';
}

/* Renderiza el mapa */
static void renderizar(const char mapa[ALTO][ANCHO + 1],
                       int jugador_x,
                       int jugador_y,
                       int pasos,
                       int monedas)
{
    consola_limpiar_simple();

    printf("PROTOTIPO 2  | Pasos: %d | Monedas: %d\n\n", pasos, monedas);

    for (int y = 0; y < ALTO; y++)
    {
        for (int x = 0; x < ANCHO; x++)
        {
            if (x == jugador_x && y == jugador_y)
                putchar('@');
            else
                putchar(mapa[y][x]);
        }
        putchar('\n');
    }

    puts("\nControles: W A S D + Enter | Q para salir");
}

int main()
{
    char mapa[ALTO][ANCHO + 1];

    /* Copiamos el mapa fijo */
    for (int y = 0; y < ALTO; y++)
        strncpy(mapa[y], MAPA_FIJO[y], ANCHO + 1);

    int xJugador = 1;
    int yJugador = 1;
    int pasos = 0;
    int monedas = 0;

    int tecla;

    while (1)
    {
        renderizar(mapa, xJugador, yJugador, pasos, monedas);

        printf(">");
        fflush(stdout);

        if (scanf(" %c", &tecla) != 1)
            break;

        if (tecla == 'q' || tecla == 'Q')
            break;

        int dx = 0, dy = 0;

        if (tecla == 'w' || tecla == 'W')
            dy = -1;
        else if (tecla == 's' || tecla == 'S')
            dy = 1;
        else if (tecla == 'a' || tecla == 'A')
            dx = -1;
        else if (tecla == 'd' || tecla == 'D')
            dx = 1;

        int nx = xJugador + dx;
        int ny = yJugador + dy;

        /* Validación de colisión */
        if (!es_pared(mapa, nx, ny))
        {
            /* Si hay moneda, se recoge */
            if (mapa[ny][nx] == '*')
            {
                monedas++;
                mapa[ny][nx] = '.'; // Elimina la moneda
                puts("¡Moneda recogida!");
            }

            xJugador = nx;
            yJugador = ny;
            pasos++;
        }
        else
        {
            puts("¡No puedes atravesar paredes!");
        }
    }

    printf("\nFin del juego.\n");
    printf("Pasos totales: %d\n", pasos);
    printf("Monedas recogidas: %d\n", monedas);

    return 0;
}
