#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <ctype.h>
#include <string.h>

#define ROWS 10
#define COLS 20

int main() {

    // Mapa tipo laberinto (como el que pediste)
    const char *MAP[ROWS] = {
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

    char board[ROWS][COLS];

    // Copiar mapa a la matriz
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            board[i][j] = MAP[i][j];
        }
    }

    // Posición inicial segura (zona libre)
    int playerX = 1;
    int playerY = 1;

    int score = 0;
    char input;
    int running = 1;

    printf("=== SIMPLE GAME LABYRINTH ===\n");
    printf("Use W/A/S/D to move, Q to quit\n\n");

    while (running) {

        system("cls");

        // Dibujar tablero
        for (int i = 0; i < ROWS; i++) {
            for (int j = 0; j < COLS; j++) {
                if (i == playerY && j == playerX) {
                    printf("@");
                } else {
                    printf("%c", board[i][j]);
                }
            }
            printf("\n");
        }

        printf("\nScore: %d | Position: (%d, %d)\n", score, playerX, playerY);
        printf("Move (W/A/S/D) or Q to quit: ");

        input = getch();
        input = toupper(input);

        int newX = playerX;
        int newY = playerY;

        if (input == 'W')
            newY--;
        else if (input == 'S')
            newY++;
        else if (input == 'A')
            newX--;
        else if (input == 'D')
            newX++;
        else if (input == 'Q')
            running = 0;

        // Validación: no atravesar paredes
        if (board[newY][newX] != '#') {

            // Si pisa moneda
            if (board[newY][newX] == '*') {
                score += 50;
                board[newY][newX] = '.'; // eliminar moneda
            } else {
                score += 10;
            }

            playerX = newX;
            playerY = newY;
        }
    }

    printf("\n=== GAME OVER ===\n");
    printf("Final Score: %d\n", score);

    return 0;
}
