#ifndef MOVE_CHECK_H
#define MOVE_CHECK_H

#include <stdint.h>
#include "parse_logic.h"
#include "chess_table.h"

#define BYTEOFFSET_BITWIZE 3

int find_king_position(uint64_t *board, uint8_t color, uint8_t king_pos[2]);
int check_rays_from_source(uint8_t source[2], uint64_t *board, uint8_t target_type, uint8_t target_color);
int is_pinned(Piece piece, uint8_t source[2], uint64_t *board);
int is_king_under_attack(uint8_t king_position[2], uint64_t *board);
int correct_move_pawn(uint8_t source[2], uint8_t destination[2], uint64_t *board, uint16_t en_passant_target);
int correct_move_knight(uint8_t source[2], uint8_t destination[2], uint64_t *board);
int correct_move_bishop(uint8_t source[2], uint8_t destination[2], uint64_t *board);
int correct_move_rook(uint8_t source[2], uint8_t destination[2], uint64_t *board);
int correct_move_queen(uint8_t source[2], uint8_t destination[2], uint64_t *board);
int correct_move_king(uint8_t source[2], uint8_t destination[2], uint64_t *board);
int correct_move_castle(Move move, uint64_t *board);
int prepare_castle_move(Move *move, uint8_t color, uint64_t *board);
void remove_figure(uint8_t position[2], uint64_t *board);
int is_valid(Move move, uint64_t *board);

#endif
