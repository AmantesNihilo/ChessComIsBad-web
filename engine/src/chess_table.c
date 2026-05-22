#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "chess_table.h"

#define TABLE_COLUMNS 8
#define ROW2 8
#define ROW7 48
#define ROW8 56


void init_chess_table(uint64_t *obj) {
    // Обнуляем все столбцы
    for (int i = 0; i < 8; i++) obj[i] = 0ULL;

    // Пешки
    for (int i = 0; i < 8; i++) {
        obj[i] |= ((uint64_t)(PAWN | WHITE) << (1 << 3)); // row=1 (вторая строка)
        obj[i] |= ((uint64_t)(PAWN | BLACK) << (6 << 3)); // row=6 (седьмая строка)
    }

    // Ладьи
    obj[0] |= ((uint64_t)(ROOK | WHITE) << (0 << 3)); // a1
    obj[7] |= ((uint64_t)(ROOK | WHITE) << (0 << 3)); // h1
    obj[0] |= ((uint64_t)(ROOK | BLACK) << (7 << 3)); // a8
    obj[7] |= ((uint64_t)(ROOK | BLACK) << (7 << 3)); // h8

    // Кони
    obj[1] |= ((uint64_t)(KNIGHT | WHITE) << (0 << 3)); // b1
    obj[6] |= ((uint64_t)(KNIGHT | WHITE) << (0 << 3)); // g1
    obj[1] |= ((uint64_t)(KNIGHT | BLACK) << (7 << 3)); // b8
    obj[6] |= ((uint64_t)(KNIGHT | BLACK) << (7 << 3)); // g8

    // Слоны
    obj[2] |= ((uint64_t)(BISHOP | WHITE) << (0 << 3)); // c1
    obj[5] |= ((uint64_t)(BISHOP | WHITE) << (0 << 3)); // f1
    obj[2] |= ((uint64_t)(BISHOP | BLACK) << (7 << 3)); // c8
    obj[5] |= ((uint64_t)(BISHOP | BLACK) << (7 << 3)); // f8

    // Ферзи
    obj[3] |= ((uint64_t)(QUEEN | WHITE) << (0 << 3)); // d1
    obj[3] |= ((uint64_t)(QUEEN | BLACK) << (7 << 3)); // d8

    // Короли
    obj[4] |= ((uint64_t)(KING | WHITE) << (0 << 3)); // e1
    obj[4] |= ((uint64_t)(KING | BLACK) << (7 << 3)); // e8

}

// Находит N-ю свободную клетку на линии.
static int find_empty(const uint8_t *row, int target) {
    int count = 0;
    for (int i = 0; i < 8; i++) {
        if (row[i] == 0) {
            if (count == target) return i;
            count++;
        }
    }
    return -1;
}

void init_fisher(uint64_t *obj, int seed) {
    if (seed < 0 || seed > 959) seed = 0;

    for (int i = 0; i < 8; i++) obj[i] = 0ULL;

    uint8_t row[8] = {0};
    int n = seed;

    int white_sq[4] = {1, 3, 5, 7};
    row[white_sq[n % 4]] = BISHOP;
    n /= 4;

    int black_sq[4] = {0, 2, 4, 6};
    row[black_sq[n % 4]] = BISHOP;
    n /= 4;

    row[find_empty(row, n % 6)] = QUEEN;
    n /= 6;

    int knights[10][2] = {
        {0, 1}, {0, 2}, {0, 3}, {0, 4}, {1, 2},
        {1, 3}, {1, 4}, {2, 3}, {2, 4}, {3, 4}
    };
    row[find_empty(row, knights[n][0])] = KNIGHT;
    row[find_empty(row, knights[n][1])] = KNIGHT;

    row[find_empty(row, 0)] = ROOK;
    row[find_empty(row, 0)] = KING;
    row[find_empty(row, 0)] = ROOK;

    for (int i = 0; i < 8; i++) {
        uint8_t piece = row[i];
        obj[i] |= ((uint64_t)(piece | WHITE) << 0);
        obj[i] |= ((uint64_t)(PAWN | WHITE) << ROW2);
        obj[i] |= ((uint64_t)(PAWN | BLACK) << ROW7);
        obj[i] |= ((uint64_t)(piece | BLACK) << ROW8);
    }
}

uint64_t *create_chess_table() {
    uint64_t *new_obj = calloc(TABLE_COLUMNS, sizeof(uint64_t));

    if (new_obj == NULL) {
        printf("Problem with allocating memmory\n");
        return NULL;
    }

    init_chess_table(new_obj);
    return new_obj;
}

void free_chess_table(uint64_t *obj) {
    free(obj);
} 
