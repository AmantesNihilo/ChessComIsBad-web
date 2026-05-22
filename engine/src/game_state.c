#include <string.h>
#include <stdlib.h>
#include "game_state.h"
#include "move_check.h"
#include "move_logic.h"

static Piece game_get_piece(uint64_t *board, uint8_t col, uint8_t row) {
    return (Piece)((board[col] >> (row << BYTEOFFSET_BITWIZE)) & 0xFF);
}

static void game_copy_board(uint64_t *dst, uint64_t *src) {
    memcpy(dst, src, sizeof(uint64_t) * 8);
}

static int game_opponent(uint8_t color) {
    return (color == WHITE) ? BLACK : WHITE;
}

static int game_piece_attacks(uint64_t *board, uint8_t src_col, uint8_t src_row,
                              uint8_t dst_col, uint8_t dst_row);

static Move game_make_move(uint8_t src_col, uint8_t src_row,
                           uint8_t dst_col, uint8_t dst_row,
                           char piece, MoveType type) {
    Move move;

    memset(&move, 0, sizeof(move));
    move.src[0] = src_col;
    move.src[1] = src_row;
    move.dst[0] = dst_col;
    move.dst[1] = dst_row;
    move.piece = piece;
    move.type = type;
    return move;
}

void game_init(GameState *game) {
    init_chess_table(game->board);
    game->current_color = WHITE;

    game->white_king_moved = 0;
    game->black_king_moved = 0;
    game->white_left_rook_moved = 0;
    game->white_right_rook_moved = 0;
    game->black_left_rook_moved = 0;
    game->black_right_rook_moved = 0;

    game->en_passant_available = 0;
    game->en_passant_target[0] = 0;
    game->en_passant_target[1] = 0;
    game->en_passant_pawn[0] = 0;
    game->en_passant_pawn[1] = 0;

    game->game_over = 0;
    game->checkmate = 0;
    game->stalemate = 0;
    game->double_check = 0;
}

void game_init_fisher(GameState *game, int seed) {
    init_fisher(game->board, seed);
    game->current_color = WHITE;

    game->white_king_moved = 0;
    game->black_king_moved = 0;
    game->white_left_rook_moved = 0;
    game->white_right_rook_moved = 0;
    game->black_left_rook_moved = 0;
    game->black_right_rook_moved = 0;

    game->en_passant_available = 0;
    game->en_passant_target[0] = 0;
    game->en_passant_target[1] = 0;
    game->en_passant_pawn[0] = 0;
    game->en_passant_pawn[1] = 0;

    game->game_over = 0;
    game->checkmate = 0;
    game->stalemate = 0;
    game->double_check = 0;
}

void game_clear(GameState *game) {
    for (int i = 0; i < 8; i++) game->board[i] = 0ULL;
    game->current_color = WHITE;

    game->white_king_moved = 0;
    game->black_king_moved = 0;
    game->white_left_rook_moved = 0;
    game->white_right_rook_moved = 0;
    game->black_left_rook_moved = 0;
    game->black_right_rook_moved = 0;

    game->en_passant_available = 0;
    game->en_passant_target[0] = 0;
    game->en_passant_target[1] = 0;
    game->en_passant_pawn[0] = 0;
    game->en_passant_pawn[1] = 0;

    game->game_over = 0;
    game->checkmate = 0;
    game->stalemate = 0;
    game->double_check = 0;
}

static int game_can_castle(GameState *game, Move move, uint8_t color) {
    if (color == WHITE) {
        if (game->white_king_moved) return 0;
        if (move.type == MOVE_LONG_CASTLE && game->white_left_rook_moved) return 0;
        if (move.type == MOVE_SHORT_CASTLE && game->white_right_rook_moved) return 0;
    } else {
        if (game->black_king_moved) return 0;
        if (move.type == MOVE_LONG_CASTLE && game->black_left_rook_moved) return 0;
        if (move.type == MOVE_SHORT_CASTLE && game->black_right_rook_moved) return 0;
    }
    return 1;
}

static void game_update_castle_rights(GameState *game, Move move, Piece source_piece, Piece target_piece) {
    uint8_t source_color = source_piece & COLOR_MASK;
    uint8_t target_color = target_piece & COLOR_MASK;

    if ((source_piece & 0x3F) == KING) {
        if (source_color == WHITE) game->white_king_moved = 1;
        else game->black_king_moved = 1;
    }

    if ((source_piece & 0x3F) == ROOK) {
        if (source_color == WHITE && move.src[0] == 0 && move.src[1] == 0) game->white_left_rook_moved = 1;
        if (source_color == WHITE && move.src[0] == 7 && move.src[1] == 0) game->white_right_rook_moved = 1;
        if (source_color == BLACK && move.src[0] == 0 && move.src[1] == 7) game->black_left_rook_moved = 1;
        if (source_color == BLACK && move.src[0] == 7 && move.src[1] == 7) game->black_right_rook_moved = 1;
    }

    if ((target_piece & 0x3F) == ROOK) {
        if (target_color == WHITE && move.dst[0] == 0 && move.dst[1] == 0) game->white_left_rook_moved = 1;
        if (target_color == WHITE && move.dst[0] == 7 && move.dst[1] == 0) game->white_right_rook_moved = 1;
        if (target_color == BLACK && move.dst[0] == 0 && move.dst[1] == 7) game->black_left_rook_moved = 1;
        if (target_color == BLACK && move.dst[0] == 7 && move.dst[1] == 7) game->black_right_rook_moved = 1;
    }
}

static int game_valid_promotion(Move move, Piece source_piece) {
    uint8_t source_color = source_piece & COLOR_MASK;
    uint8_t last_row = (source_color == WHITE) ? 7 : 0;

    if ((source_piece & 0x3F) != PAWN) return move.promotion == 0;
    if (move.dst[1] != last_row) return move.promotion == 0;
    return move.promotion == QUEEN ||
           move.promotion == ROOK ||
           move.promotion == BISHOP ||
           move.promotion == KNIGHT;
}

static int game_valid_en_passant(GameState *game, Move move, Piece source_piece) {
    Piece captured_piece;

    if (!move.en_passant) return 1;
    if ((source_piece & 0x3F) != PAWN) return 0;
    if (move.type != MOVE_CAPTURE) return 0;
    if (!game->en_passant_available) return 0;
    if (move.dst[0] != game->en_passant_target[0] ||
        move.dst[1] != game->en_passant_target[1]) return 0;

    captured_piece = game_get_piece(game->board, game->en_passant_pawn[0],
                                    game->en_passant_pawn[1]);
    if ((captured_piece & 0x3F) != PAWN) return 0;
    return (captured_piece & COLOR_MASK) != (source_piece & COLOR_MASK);
}

static int game_valid_move_type(GameState *game, Move move,
                                Piece source_piece, Piece target_piece) {
    if ((source_piece & 0x3F) != move.piece) return 0;
    if (!game_valid_promotion(move, source_piece)) return 0;
    if (!game_valid_en_passant(game, move, source_piece)) return 0;

    if (move.type == MOVE_CAPTURE) {
        if (move.en_passant) return target_piece == EMPTY;
        return target_piece != EMPTY &&
               (target_piece & COLOR_MASK) != (source_piece & COLOR_MASK);
    }

    if (move.type == MOVE_NORMAL) {
        return target_piece == EMPTY && !move.en_passant;
    }

    return 1;
}

static void game_update_en_passant(GameState *game, Move move, Piece source_piece) {
    int diff_row = (int)move.dst[1] - (int)move.src[1];

    game->en_passant_available = 0;
    if ((source_piece & 0x3F) == PAWN && abs(diff_row) == 2) {
        game->en_passant_available = 1;
        game->en_passant_target[0] = move.src[0];
        game->en_passant_target[1] = (uint8_t)((move.src[1] + move.dst[1]) / 2);
        game->en_passant_pawn[0] = move.dst[0];
        game->en_passant_pawn[1] = move.dst[1];
    }
}

static int game_move_is_legal(GameState *game, Move move, uint8_t color) {
    uint64_t temp_board[8];
    uint8_t king_pos[2];
    Piece source_piece = game_get_piece(game->board, move.src[0], move.src[1]);

    if (source_piece == EMPTY) return 0;
    if ((source_piece & COLOR_MASK) != color) return 0;
    if (!is_valid(move, game->board)) return 0;

    game_copy_board(temp_board, game->board);
    apply_move(move, temp_board);

    if (!find_king_position(temp_board, color, king_pos)) return 0;
    if (is_king_under_attack(king_pos, temp_board)) return 0;

    return 1;
}

static int game_try_candidate(GameState *game, Move move, uint8_t color) {
    if (move.dst[0] > 7 || move.dst[1] > 7) return 0;
    return game_move_is_legal(game, move, color);
}

int game_has_legal_moves(GameState *game, uint8_t color) {
    for (uint8_t col = 0; col < 8; col++) {
        for (uint8_t row = 0; row < 8; row++) {
            Piece piece = game_get_piece(game->board, col, row);
            uint8_t type = piece & 0x3F;

            if (piece == EMPTY || (piece & COLOR_MASK) != color) continue;

            if (type == PAWN) {
                int8_t dir = (color == WHITE) ? 1 : -1;
                uint8_t start_row = (color == WHITE) ? 1 : 6;
                Move move = game_make_move(col, row, col, (uint8_t)(row + dir),
                                           PAWN, MOVE_NORMAL);
                if (row + dir >= 0 && row + dir < 8 && game_try_candidate(game, move, color)) return 1;
                move.dst[1] = (uint8_t)(row + dir * 2);
                if (row == start_row && game_try_candidate(game, move, color)) return 1;
                if (col > 0 && row + dir >= 0 && row + dir < 8) {
                    move.dst[0] = col - 1; move.dst[1] = (uint8_t)(row + dir); move.type = MOVE_CAPTURE;
                    if (game_try_candidate(game, move, color)) return 1;
                }
                if (col < 7 && row + dir >= 0 && row + dir < 8) {
                    move.dst[0] = col + 1; move.dst[1] = (uint8_t)(row + dir); move.type = MOVE_CAPTURE;
                    if (game_try_candidate(game, move, color)) return 1;
                }
            } else if (type == KNIGHT) {
                int8_t dc[] = {2, 2, -2, -2, 1, 1, -1, -1};
                int8_t dr[] = {1, -1, 1, -1, 2, -2, 2, -2};
                for (int i = 0; i < 8; i++) {
                    int c = col + dc[i];
                    int r = row + dr[i];
                    Move move = game_make_move(col, row, (uint8_t)c, (uint8_t)r,
                                               KNIGHT, MOVE_NORMAL);
                    if (c >= 0 && c < 8 && r >= 0 && r < 8 && game_try_candidate(game, move, color)) return 1;
                }
            } else if (type == KING) {
                for (int8_t dc = -1; dc <= 1; dc++) {
                    for (int8_t dr = -1; dr <= 1; dr++) {
                        int c = col + dc;
                        int r = row + dr;
                        Move move = game_make_move(col, row, (uint8_t)c, (uint8_t)r,
                                                   KING, MOVE_NORMAL);
                        if ((dc != 0 || dr != 0) && c >= 0 && c < 8 && r >= 0 && r < 8 &&
                            game_try_candidate(game, move, color)) return 1;
                    }
                }
            } else {
                int8_t dirs_c[] = {0, 0, -1, 1, -1, 1, -1, 1};
                int8_t dirs_r[] = {1, -1, 0, 0, 1, 1, -1, -1};
                for (int i = 0; i < 8; i++) {
                    if (type == ROOK && i >= 4) continue;
                    if (type == BISHOP && i < 4) continue;
                    for (int step = 1; step < 8; step++) {
                        int c = col + dirs_c[i] * step;
                        int r = row + dirs_r[i] * step;
                        Move move = game_make_move(col, row, (uint8_t)c, (uint8_t)r,
                                                   (char)type, MOVE_NORMAL);
                        if (c < 0 || c >= 8 || r < 0 || r >= 8) break;
                        if (game_try_candidate(game, move, color)) return 1;
                    }
                }
            }
        }
    }

    return 0;
}

int game_count_attackers(GameState *game, uint8_t color) {
    uint8_t king_pos[2];
    int count = 0;

    if (!find_king_position(game->board, color, king_pos)) return 0;

    for (uint8_t col = 0; col < 8; col++) {
        for (uint8_t row = 0; row < 8; row++) {
            Piece piece = game_get_piece(game->board, col, row);

            if (piece == EMPTY || (piece & COLOR_MASK) != game_opponent(color)) continue;

            if (game_piece_attacks(game->board, col, row, king_pos[0], king_pos[1])) count++;
        }
    }

    return count;
}

int game_is_checkmate(GameState *game, uint8_t color) {
    uint8_t king_pos[2];

    if (!find_king_position(game->board, color, king_pos)) return 0;
    if (!is_king_under_attack(king_pos, game->board)) return 0;
    return !game_has_legal_moves(game, color);
}

int game_is_stalemate(GameState *game, uint8_t color) {
    uint8_t king_pos[2];

    if (!find_king_position(game->board, color, king_pos)) return 0;
    if (is_king_under_attack(king_pos, game->board)) return 0;
    return !game_has_legal_moves(game, color);
}

static int game_clear_path(uint64_t *board, uint8_t src_col, uint8_t src_row,
                           uint8_t dst_col, uint8_t dst_row) {
    int8_t diff_col = (int8_t)((int)dst_col - (int)src_col);
    int8_t diff_row = (int8_t)((int)dst_row - (int)src_row);
    int8_t step_col = (diff_col > 0) ? 1 : (diff_col < 0 ? -1 : 0);
    int8_t step_row = (diff_row > 0) ? 1 : (diff_row < 0 ? -1 : 0);
    int8_t c = (int8_t)src_col + step_col;
    int8_t r = (int8_t)src_row + step_row;

    while (c != (int8_t)dst_col || r != (int8_t)dst_row) {
        if (game_get_piece(board, (uint8_t)c, (uint8_t)r) != EMPTY) return 0;
        c += step_col; r += step_row;
    }
    return 1;
}

static int game_piece_attacks(uint64_t *board, uint8_t src_col, uint8_t src_row,
                              uint8_t dst_col, uint8_t dst_row) {
    Piece piece = game_get_piece(board, src_col, src_row);
    uint8_t type = piece & 0x3F;
    int8_t diff_col = (int8_t)((int)dst_col - (int)src_col);
    int8_t diff_row = (int8_t)((int)dst_row - (int)src_row);

    if (type == PAWN) {
        int8_t dir = ((piece & COLOR_MASK) == WHITE) ? 1 : -1;
        return abs((int)diff_col) == 1 && diff_row == dir;
    }
    if (type == KNIGHT) {
        return (abs((int)diff_col) == 2 && abs((int)diff_row) == 1) ||
               (abs((int)diff_col) == 1 && abs((int)diff_row) == 2);
    }
    if (type == KING) {
        return abs((int)diff_col) <= 1 && abs((int)diff_row) <= 1 &&
               !(diff_col == 0 && diff_row == 0);
    }
    if (type == ROOK) {
        return (diff_col == 0 || diff_row == 0) &&
               !(diff_col == 0 && diff_row == 0) &&
               game_clear_path(board, src_col, src_row, dst_col, dst_row);
    }
    if (type == BISHOP) {
        return abs((int)diff_col) == abs((int)diff_row) &&
               diff_col != 0 &&
               game_clear_path(board, src_col, src_row, dst_col, dst_row);
    }
    if (type == QUEEN) {
        return ((diff_col == 0 || diff_row == 0) ||
                abs((int)diff_col) == abs((int)diff_row)) &&
               !(diff_col == 0 && diff_row == 0) &&
               game_clear_path(board, src_col, src_row, dst_col, dst_row);
    }
    return 0;
}

int game_execute_parsed_move(GameState *game, Move move) {
    GameState old_game;
    Piece source_piece;
    Piece target_piece;
    uint8_t next_color;
    int gives_check;
    int ret;

    if (game->game_over) return -4;
    old_game = *game;

    if (move.type == MOVE_SHORT_CASTLE || move.type == MOVE_LONG_CASTLE) {
        if (!game_can_castle(game, move, game->current_color)) return -1;
        if (!prepare_castle_move(&move, game->current_color, game->board)) return -1;
    }

    source_piece = game_get_piece(game->board, move.src[0], move.src[1]);
    target_piece = game_get_piece(game->board, move.dst[0], move.dst[1]);

    if (source_piece == EMPTY) return -1;
    if ((source_piece & COLOR_MASK) != game->current_color) return -1;
    if (!game_valid_move_type(game, move, source_piece, target_piece)) return -1;

    ret = execute_parsed_move(move, game->board);
    if (ret != 0) return ret;

    game_update_castle_rights(game, move, source_piece, target_piece);
    game_update_en_passant(game, move, source_piece);

    next_color = (uint8_t)game_opponent(game->current_color);
    game->current_color = next_color;
    game->double_check = game_count_attackers(game, next_color) > 1;
    gives_check = game_count_attackers(game, next_color) > 0;

    if (game_is_checkmate(game, next_color)) {
        game->game_over = 1;
        game->checkmate = 1;
    } else if (game_is_stalemate(game, next_color)) {
        game->game_over = 1;
        game->stalemate = 1;
    }

    if ((move.check && (!gives_check || game->checkmate)) ||
        (move.mate && !game->checkmate)) {
        *game = old_game;
        return -1;
    }

    return 0;
}

int game_execute_move(GameState *game, const char *input) {
    Move move = parse_string_to_move(input);
    if (move.type == MOVE_ERROR) return -1;
    return game_execute_parsed_move(game, move);
}
