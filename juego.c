#include "juego.h"
#include <string.h>

// Mapa base fijo

static const char *MAPA_FIJO[ALTO] = {
    "###################",
    "#                 #",
    "#  #####          #",
    "#      #          #",
    "#      #   #####  #",
    "#          #      #",
    "#          #      #",
    "#   #####  #      #",
    "#                 #",
    "###################"};

int juego_es_pared(const Juego *j, int x, int y)
{
    // Fuera de mapa => pared
    if (x < 0 || x >= ANCHO || y >= ALTO)
        return 1;
    return (j->mapa[y][x] == '#');
}

void juego_inicializar(Juego *j)
{
    for (int y = 0; y < ALTO; y++)
    {
        strncpy(j->mapa[y], MAPA_FIJO[y], ANCHO);
        j->mapa[y][ANCHO] = '\0'; // Asegurar terminacion nula
    }
    // Posicion inicial del jugador
    j->jugador_x = 1;
    j->jugador_y = 1;

    // Si alguien cambia el mapa y esta posición cae
    // en una pared, buscamos una alternativa simple
    //(primera celda transitable)

    if (juego_es_pared(j, j->jugador_x, j->jugador_y))
    {
        for (int y = 1; y < ALTO - 1; y++)
        {
            for (int x = 1; x < ANCHO - 1; x++)
            {
                if (!juego_es_pared(j, x, y))
                {
                    j->jugador_x = x;
                    j->jugador_y = y;
                    y = ALTO;
                    break; // Salir del ciclo externo
                }
            }
        }
    }
    j->pasos = 0;
}
void juego_intentar_mover(Juego *j, int dx, int dy)
{
    // Calculamos posicion destino
    int nx = j->jugador_x + dx;
    int ny = j->jugador_y + dy;
    // Verificamos colision: si es pared, no hacemos nada
    if (juego_es_pared(j, nx, ny))
        return;

    // Si no es pared, actualizamos posicion del jugador
    j->jugador_x = nx;
    j->jugador_y = ny;
    j->pasos++;
}
