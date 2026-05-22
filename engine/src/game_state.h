#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <stdint.h>
#include "chess_table.h"
#include "parse_logic.h"

typedef struct {
    uint64_t board[8];
    uint8_t current_color;

    int white_king_moved;
    int black_king_moved;
    int white_left_rook_moved;
    int white_right_rook_moved;
    int black_left_rook_moved;
    int black_right_rook_moved;

    int en_passant_available;
    uint8_t en_passant_target[2];
    uint8_t en_passant_pawn[2];

    int game_over;
    int checkmate;
    int stalemate;
    int double_check;
} GameState;

void game_init(GameState *game);
void game_init_fisher(GameState *game, int seed);
void game_clear(GameState *game);
int game_execute_move(GameState *game, const char *input);
int game_execute_parsed_move(GameState *game, Move move);
int game_has_legal_moves(GameState *game, uint8_t color);
int game_is_checkmate(GameState *game, uint8_t color);
int game_is_stalemate(GameState *game, uint8_t color);
int game_count_attackers(GameState *game, uint8_t color);

#endif
