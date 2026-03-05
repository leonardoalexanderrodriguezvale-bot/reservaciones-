#include "juego.h"
#include "render.h"
#include "entrada.h"
#include "consola.h"

//Funcion auxiliar para convertir una tecla a desplazamiento
static int tecla_a_vector(char tecla, int *dx, int *dy){
    *dx = 0;
    *dy = 0;

    if(tecla == 'w' || tecla == 'W'){
        *dy = -1;
        return 1;
    } else if(tecla == 's' || tecla == 'S'){
        *dy = 1;
        return 1;
    } else if(tecla == 'a' || tecla == 'A'){
        *dx = -1;
        return 1;
    } else if(tecla == 'd' || tecla == 'D'){
        *dx = 1;
        return 1;
    }
    return 0; // Tecla no reconocida
}

int main (void){
    Juego juego;
    juego_inicializar(&juego);

    while(1){
        render_juego(&juego);
        char tecla = entrada_leer_tecla();
        //Condicion de salida
        if(tecla == 'q' || tecla == 'Q'){
            break;
        }
        //Calculo de movimiento
        int dx, dy;
        if(tecla_a_vector(tecla, &dx, &dy)){
            juego_intentar_mover(&juego, dx, dy);
        }
        //Si la tecla no es valida o reconocida, no pasa nada
    }
    return 0;
}