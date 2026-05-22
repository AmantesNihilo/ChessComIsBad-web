#ifndef PARSE_LOGIC_H
#define PARSE_LOGIC_H
#include <stdlib.h>
#include <stdint.h>
#include <regex.h>


typedef enum {
    MOVE_NORMAL,
    MOVE_CAPTURE,
    MOVE_LONG_CASTLE,
    MOVE_SHORT_CASTLE,
    MOVE_ERROR
} MoveType;

typedef struct {
    uint8_t src[2];  // {file, rank}
    uint8_t dst[2];
    char piece;      // 'K', 'Q', 'R', 'B', 'N', 'p' - pawn
    char promotion;
    int en_passant;
    int check;
    int mate;
    MoveType type;
} Move;



Move parse_string_to_move(const char *string);
int is_valid_move(regmatch_t groups[6], uint8_t source[2], uint8_t destination[2], uint64_t *board);
int execute_move(const char* input, uint64_t *board);
#endif
