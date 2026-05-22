#include "move_check.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "chess_table.h"

#define MOVE_CHECK_FAILURE(msg) do { \
    printf("[MOVE_CHECK_ERROR]: %s\n", msg); \
    return 1; \
} while(0)

int find_king_position(uint64_t *board, uint8_t color, uint8_t king_pos[2]) {
    for (uint8_t col = 0; col < 8; col++) {
        for (uint8_t row = 0; row < 8; row++) {
            Piece p = (board[col] >> (row << BYTEOFFSET_BITWIZE)) & 0xFF;
            if ((p & 0x3F) == KING && (p & COLOR_MASK) == color) {
                king_pos[0] = col;
                king_pos[1] = row;
                return 1;
            }
        }
    }
    return 0;
}

int check_rays_from_source(uint8_t source[2], uint64_t *board, uint8_t target_type, uint8_t target_color) {
    int8_t dir_c[] = {0, 0, -1, 1, -1, 1, -1, 1};
    int8_t dir_r[] = {1, -1, 0, 0, 1, 1, -1, -1};

    for (int i = 0; i < 8; i++) {
        if (target_type == ROOK && i >= 4) continue;
        if (target_type == BISHOP && i < 4) continue;

        // Используем знаковый int8_t для предотвращения overflow
        int8_t c = (int8_t)source[0] + dir_c[i];
        int8_t r = (int8_t)source[1] + dir_r[i];

        while (c >= 0 && c < 8 && r >= 0 && r < 8) {
            Piece p = (board[c] >> (r << BYTEOFFSET_BITWIZE)) & 0xFF;
            if (p != EMPTY) {
                if ((p & 0x3F) == target_type && (p & COLOR_MASK) == target_color) return 1;
                break; // Луч упёрся в другую фигуру
            }
            c += dir_c[i];
            r += dir_r[i];
        }
    }
    return 0;
}

int is_pinned(Piece piece, uint8_t source[2], uint64_t *board) {
    uint8_t color = piece & COLOR_MASK;
    uint8_t opponent = (color == WHITE) ? BLACK : WHITE;
    uint8_t king_pos[2];

    if (!find_king_position(board, color, king_pos)) return 0;

    int8_t diff_c = (int8_t)source[0] - king_pos[0];
    int8_t diff_r = (int8_t)source[1] - king_pos[1];

    // Если не на одной линии с королем — пригвождения нет
    if (abs(diff_c) != abs(diff_r) && diff_c != 0 && diff_r != 0) return 0;

    int8_t step_c = (diff_c > 0) ? 1 : (diff_c < 0 ? -1 : 0);
    int8_t step_r = (diff_r > 0) ? 1 : (diff_r < 0 ? -1 : 0);

    int pieces_between = 0;
    int8_t c = (int8_t)king_pos[0] + step_c;
    int8_t r = (int8_t)king_pos[1] + step_r;

    while (c >= 0 && c < 8 && r >= 0 && r < 8) {
        if (c == source[0] && r == source[1]) {
            c += step_c; r += step_r;
            continue;
        }

        Piece p = (board[c] >> (r << BYTEOFFSET_BITWIZE)) & 0xFF;
        if (p != EMPTY) {
            pieces_between++;
            if (pieces_between > 1) return 0; // Больше одной фигуры между королем и атакующим

            if ((p & COLOR_MASK) == opponent) {
                uint8_t type = p & 0x3F;
                // Проверка: может ли враг атаковать по этой линии
                if ((step_c == 0 || step_r == 0) && (type == ROOK || type == QUEEN)) return 1;
                if ((step_c != 0 && step_r != 0) && (type == BISHOP || type == QUEEN)) return 1;
            } else {
                return 0; // Своя фигура защищает
            }
        }
        c += step_c;
        r += step_r;
    }
    return 0;
}


int is_king_under_attack(uint8_t king_position[2], uint64_t *board) {
    Piece king = (board[king_position[0]] >> (king_position[1] << BYTEOFFSET_BITWIZE)) & 0xFF;
    uint8_t king_color = king & COLOR_MASK;
    uint8_t opponent_color = (king_color == WHITE) ? BLACK : WHITE;

    if (check_rays_from_source(king_position, board, ROOK, opponent_color) ||
        check_rays_from_source(king_position, board, BISHOP, opponent_color) ||
        check_rays_from_source(king_position, board, QUEEN, opponent_color)) {
        return 1;
    }

    int8_t knight_diff_col[] = {2, 2, -2, -2, 1, 1, -1, -1};
    int8_t knight_diff_row[] = {1, -1, 1, -1, 2, -2, 2, -2};
    for (int i = 0; i < 8; i++) {
        int r = king_position[1] + knight_diff_row[i];
        int c = king_position[0] + knight_diff_col[i];
        if (r >= 0 && r < 8 && c >= 0 && c < 8) {
            Piece p = (board[c] >> (r << BYTEOFFSET_BITWIZE)) & 0xFF;
            if ((p & 0x3F) == KNIGHT && (p & COLOR_MASK) == opponent_color) {
                return 1;
            }
        }
    }

    int8_t pawn_dir = (king_color == WHITE) ? -1 : 1;
    int8_t pawn_cols[] = {-1, 1};
    for (int i = 0; i < 2; i++) {
        int r = king_position[1] + pawn_dir;
        int c = king_position[0] + pawn_cols[i];
        if (r >= 0 && r < 8 && c >= 0 && c < 8) {
            Piece p = (board[c] >> (r << BYTEOFFSET_BITWIZE)) & 0xFF;
            if ((p & 0x3F) == PAWN && (p & COLOR_MASK) == opponent_color) {
                return 1;
            }
        }
    }

    int8_t king_diff_col[] = {0, 0, 1, -1, 1, -1, 1, -1};
    int8_t king_diff_row[] = {1, -1, 0, 0, 1, 1, -1, -1};
    for (int i = 0; i < 8; i++) {
        int r = king_position[1] + king_diff_row[i];
        int c = king_position[0] + king_diff_col[i];
        if (r >= 0 && r < 8 && c >= 0 && c < 8) {
            Piece p = (board[c] >> (r << BYTEOFFSET_BITWIZE)) & 0xFF;
            if ((p & 0x3F) == KING && (p & COLOR_MASK) == opponent_color) {
                return 1;
            }
        }
    }
    return 0;
}

int correct_move_pawn(uint8_t source[2], uint8_t destination[2], uint64_t *board, uint16_t en_passant_target) {
    uint64_t source_column_data = board[source[0]];
    Piece pawn = (source_column_data >> (source[1] << BYTEOFFSET_BITWIZE)) & 0xFF;
    
    if (is_pinned(pawn, source, board) == 1) {
        return 0;
    }
    
    uint8_t is_white = !(pawn & BLACK);
    int8_t  direction = is_white ? 1 : -1;
    uint8_t start_row = is_white ? 1 : 6;

    int8_t difference_column = destination[0] - source[0];
    int8_t difference_row = destination[1] - source[1];
    
    if (difference_column == 0) {
        uint64_t destination_column_data = board[destination[0]];
        Piece target = (destination_column_data >> (destination[1] << BYTEOFFSET_BITWIZE)) & 0xFF;

        if (target != EMPTY) {
            
            return 0;
        }

        if (difference_row == direction) {
            return 1;
        }

        if ((difference_row == 2 * direction) && (source[1] == start_row)) {
            uint64_t middle_target = (source_column_data >> ((source[1] + direction) << BYTEOFFSET_BITWIZE)) & 0xFF;
            if (middle_target != EMPTY) {
                
                return 0;
            }
            return 1;
        }
    } else if ((abs((int)difference_column) == 1) && (difference_row == direction)) {
        uint64_t destination_column_data = board[destination[0]];
        Piece target = (destination_column_data >> (destination[1] << BYTEOFFSET_BITWIZE)) & 0xFF;

        if (target != EMPTY) {
            if (!(target & (is_white ? WHITE : BLACK))) {
                if ((target & 0x3F) == KING) return 0; 
                return 1;
            }
        } else {
            if (en_passant_target == (destination[0] | (destination[1] << 8))) {
                return 1;
            }
        }
    }
    return 0;
}

int correct_move_knight(uint8_t source[2], uint8_t destination[2], uint64_t *board) {
    uint64_t source_column_data = board[source[0]];
    Piece knight = (source_column_data >> (source[1] << BYTEOFFSET_BITWIZE)) & 0xFF;
    
    if (is_pinned(knight, source, board) == 1) return 0;

    int8_t difference_column = destination[0] - source[0];
    int8_t difference_row = destination[1] - source[1];

    if (((abs((int)difference_column) == 2 && abs((int)difference_row) == 1) || 
        (abs((int)difference_column) == 1 && abs((int)difference_row) == 2))) {
        
        uint64_t destination_column_data = board[destination[0]];
        Piece target = (destination_column_data >> (destination[1] << BYTEOFFSET_BITWIZE)) & 0xFF;
        
        if (target & KING) {
            
            return 0;
        }
        else if ((target == EMPTY) || ((knight & COLOR_MASK) != (target & COLOR_MASK))) {
            return 1;
        }
        else if ((target != EMPTY) && ((knight & COLOR_MASK) == (target & COLOR_MASK))) {
            return 0;
        }
    }
    return 0;
}

int correct_move_rook(uint8_t source[2], uint8_t destination[2], uint64_t *board) {
    uint64_t source_column_data = board[source[0]];
    Piece rook = (source_column_data >> (source[1] << BYTEOFFSET_BITWIZE)) & 0xFF;

    if (is_pinned(rook, source, board) == 1) return 0;

    int8_t diff_col = (int8_t)((int)destination[0] - (int)source[0]);
    int8_t diff_row = (int8_t)((int)destination[1] - (int)source[1]);

    if ((diff_col == 0 || diff_row == 0) && !(diff_col == 0 && diff_row == 0)) {
        int8_t step_col = (diff_col > 0) ? 1 : (diff_col < 0 ? -1 : 0);
        int8_t step_row = (diff_row > 0) ? 1 : (diff_row < 0 ? -1 : 0);
        int8_t c = (int8_t)source[0] + step_col;
        int8_t r = (int8_t)source[1] + step_row;

        while (c != (int8_t)destination[0] || r != (int8_t)destination[1]) {
            Piece p = (board[c] >> (r << BYTEOFFSET_BITWIZE)) & 0xFF;
            if (p != EMPTY) return 0;
            c += step_col; r += step_row;
        }

        Piece target = (board[destination[0]] >> (destination[1] << BYTEOFFSET_BITWIZE)) & 0xFF;
        if (target & KING)                                  return 0;
        if (target == EMPTY)                                return 1;
        if ((rook & COLOR_MASK) != (target & COLOR_MASK))  return 1;
    }
    return 0;
}

int correct_move_bishop(uint8_t source[2], uint8_t destination[2], uint64_t *board) {
    uint64_t source_column_data = board[source[0]];
    Piece bishop = (source_column_data >> (source[1] << BYTEOFFSET_BITWIZE)) & 0xFF;
    
    if (is_pinned(bishop, source, board) == 1) return 0;

    int8_t diff_col = (int8_t)((int)destination[0] - (int)source[0]);
    int8_t diff_row = (int8_t)((int)destination[1] - (int)source[1]);

    if (abs((int)diff_col) == abs((int)diff_row)) {
        int8_t step_col = (diff_col > 0) ? 1 : -1;
        int8_t step_row = (diff_row > 0) ? 1 : -1;
        int8_t c = (int8_t)source[0] + step_col;
        int8_t r = (int8_t)source[1] + step_row;

        while (c != (int8_t)destination[0] || r != (int8_t)destination[1]) {
            Piece p = (board[c] >> (r << BYTEOFFSET_BITWIZE)) & 0xFF;
            if (p != EMPTY) return 0;
            c += step_col; r += step_row;
        }

        Piece target = (board[destination[0]] >> (destination[1] << BYTEOFFSET_BITWIZE)) & 0xFF;
        if (target & KING)                                     return 0;
        if (target == EMPTY)                                   return 1;
        if ((bishop & COLOR_MASK) != (target & COLOR_MASK))   return 1;
    }
    return 0;
}

int correct_move_queen(uint8_t source[2], uint8_t destination[2], uint64_t *board) {
    uint64_t source_column_data = board[source[0]];
    Piece queen = (source_column_data >> (source[1] << BYTEOFFSET_BITWIZE)) & 0xFF;
    
    if (is_pinned(queen, source, board) == 1) return 0;

    int8_t diff_col = (int8_t)((int)destination[0] - (int)source[0]);
    int8_t diff_row = (int8_t)((int)destination[1] - (int)source[1]);

    if ((abs((int)diff_col) == abs((int)diff_row)) ||
        (diff_col == 0 || diff_row == 0)) {
        int8_t step_col = (diff_col > 0) ? 1 : (diff_col < 0 ? -1 : 0);
        int8_t step_row = (diff_row > 0) ? 1 : (diff_row < 0 ? -1 : 0);
        int8_t c = (int8_t)source[0] + step_col;
        int8_t r = (int8_t)source[1] + step_row;

        while (c != (int8_t)destination[0] || r != (int8_t)destination[1]) {
            Piece p = (board[c] >> (r << BYTEOFFSET_BITWIZE)) & 0xFF;
            if (p != EMPTY) return 0;
            c += step_col; r += step_row;
        }

        Piece target = (board[destination[0]] >> (destination[1] << BYTEOFFSET_BITWIZE)) & 0xFF;
        if (target & KING)                                    return 0;
        if (target == EMPTY)                                  return 1;
        if ((queen & COLOR_MASK) != (target & COLOR_MASK))   return 1;
    }
    return 0;
}

int correct_move_king(uint8_t source[2], uint8_t destination[2], uint64_t *board) {
    uint64_t source_column_data = board[source[0]];
    Piece king = (source_column_data >> (source[1] << BYTEOFFSET_BITWIZE)) & 0xFF;

    int8_t difference_column = destination[0] - source[0];
    int8_t difference_row = destination[1] - source[1];

    if ((abs((int)difference_column) < 2 && abs((int)difference_row) < 2)) {
        uint64_t destination_column_data = board[destination[0]];
        Piece target = (destination_column_data >> (destination[1] << BYTEOFFSET_BITWIZE)) & 0xFF;
        
        if (target & KING) {
            return 0;
        }

        uint8_t target_color = target & COLOR_MASK;
        uint8_t own_color = king & COLOR_MASK;

        if (target == EMPTY || target_color != own_color) {
            return 1;
        }
    }
    return 0;
}

int correct_move_castle(Move move, uint64_t *board) {
    uint8_t row = move.src[1];
    uint8_t king_col = 4;
    uint8_t rook_col = (move.type == MOVE_SHORT_CASTLE) ? 7 : 0;
    uint8_t king_dst = (move.type == MOVE_SHORT_CASTLE) ? 6 : 2;
    int8_t step = (move.type == MOVE_SHORT_CASTLE) ? 1 : -1;

    if (move.src[0] != king_col || move.dst[0] != king_dst || move.dst[1] != row) return 0;

    Piece king = (board[king_col] >> (row << BYTEOFFSET_BITWIZE)) & 0xFF;
    Piece rook = (board[rook_col] >> (row << BYTEOFFSET_BITWIZE)) & 0xFF;
    uint8_t color = king & COLOR_MASK;

    if ((king & 0x3F) != KING) return 0;
    if ((rook & 0x3F) != ROOK || (rook & COLOR_MASK) != color) return 0;

    int8_t c = (int8_t)king_col + step;
    while (c != (int8_t)rook_col) {
        Piece p = (board[c] >> (row << BYTEOFFSET_BITWIZE)) & 0xFF;
        if (p != EMPTY) return 0;
        c += step;
    }

    if (is_king_under_attack(move.src, board)) return 0;

    c = (int8_t)king_col + step;
    while (c != (int8_t)king_dst + step) {
        uint64_t temp_board[8];
        for (int i = 0; i < 8; i++) temp_board[i] = board[i];

        temp_board[king_col] &= ~(0xFFULL << (row << BYTEOFFSET_BITWIZE));
        temp_board[c] &= ~(0xFFULL << (row << BYTEOFFSET_BITWIZE));
        temp_board[c] |= ((uint64_t)king << (row << BYTEOFFSET_BITWIZE));

        uint8_t king_position[2] = {(uint8_t)c, row};
        if (is_king_under_attack(king_position, temp_board)) return 0;
        c += step;
    }

    return 1;
}

int prepare_castle_move(Move *move, uint8_t color, uint64_t *board) {
    uint8_t row = (color == BLACK) ? 7 : 0;

    move->src[0] = 4;
    move->src[1] = row;
    move->dst[0] = (move->type == MOVE_SHORT_CASTLE) ? 6 : 2;
    move->dst[1] = row;
    move->piece = KING;

    return correct_move_castle(*move, board);
}

int is_valid(Move move, uint64_t *board) {
    switch (move.type) {
        case MOVE_NORMAL:
        case MOVE_CAPTURE:
            switch (move.piece) {
                case PAWN:
                    return correct_move_pawn(
                        move.src, move.dst, board,
                        move.en_passant ? (uint16_t)(move.dst[0] | (move.dst[1] << 8)) : 0);
                case KNIGHT:
                    return correct_move_knight(move.src, move.dst, board);
                case BISHOP:
                    return correct_move_bishop(move.src, move.dst, board);
                case ROOK:
                    return correct_move_rook(move.src, move.dst, board);
                case QUEEN:
                    return correct_move_queen(move.src, move.dst, board);
                case KING:
                    return correct_move_king(move.src, move.dst, board);
                default:
                    return 0;
            }
        case MOVE_SHORT_CASTLE:
        case MOVE_LONG_CASTLE:
            return correct_move_castle(move, board);
        default:
            return 0;
    }
}
