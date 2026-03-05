#include "entrada.h"
#include <stdio.h>

char entrada_leer_tecla(void)
{
    char tecla;
    //%c ignora espacios y saltos de linea anteriores
    if (scanf("%c", &tecla) != 1)
    {
        return 'q';
    }
    return tecla;
}
