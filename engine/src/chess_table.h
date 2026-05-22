#ifndef CHESS_TABLE_H
#define CHESS_TABLE_H

#include <stdint.h>

typedef enum {
    EMPTY       = 0,         // 0000 0000
    PAWN        = 1 << 0,    // 0000 0001
    KNIGHT      = 1 << 1,    // 0000 0010
    BISHOP      = 1 << 2,    // 0000 0100
    ROOK        = 1 << 3,    // 0000 1000
    QUEEN       = 1 << 4,    // 0001 0000
    KING        = 1 << 5,    // 0010 0000

    WHITE       = 1 << 6,    // 0100 0000
    BLACK       = 1 << 7,    // 1000 0000
    COLOR_MASK  = (1 << 6) | (1 << 7),
} Piece;


#ifdef __cplusplus
extern "C" {
#endif

void init_chess_table(uint64_t *obj);
void init_fisher(uint64_t *obj, int seed);
uint64_t *create_chess_table();
void free_chess_table(uint64_t *obj);

#ifdef __cplusplus
}
#endif
#endif
