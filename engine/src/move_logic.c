#include <string.h>
#include <stdio.h>
#include "move_logic.h"
#include "move_check.h"
#include "parse_logic.h"

int execute_move(const char* input, uint64_t *board) {
    Move m = parse_string_to_move(input);
    if (m.type == MOVE_ERROR) return -1;

    if (m.type == MOVE_SHORT_CASTLE || m.type == MOVE_LONG_CASTLE) {
        if (!prepare_castle_move(&m, WHITE, board) && !prepare_castle_move(&m, BLACK, board)) return -1;
    }

    return execute_parsed_move(m, board);
}

int execute_parsed_move(Move m, uint64_t *board) {
    // 1. Определяем, чья фигура стоит на src
    Piece p_src = (board[m.src[0]] >> (m.src[1] << BYTEOFFSET_BITWIZE)) & 0xFF;
    if (p_src == EMPTY) {
        fprintf(stderr, "Ошибка: Клетка %c%d пуста!\n", m.src[0] + 'a', m.src[1] + 1);
        return -1;
    }
    uint8_t my_color = p_src & COLOR_MASK;

    // 2. Базовая проверка (ходит ли так фигура вообще)
    if (!is_valid(m, board)) return -1;

    // 3. Симуляция хода на копии доски
    uint64_t temp_board[8];
    memcpy(temp_board, board, sizeof(uint64_t) * 8);
    apply_move(m, temp_board);

    // 4. Проверка: не подставили ли мы своего короля?
    uint8_t king_pos[2];
    if (!find_king_position(temp_board, my_color, king_pos)) {
        // Если короля нет на доске - это критическая ошибка состояния
        return -3;
    }

    if (is_king_under_attack(king_pos, temp_board)) {
        printf("Нелегальный ход: король оказывается под шахом!\n");
        return -2;
    }

    // 5. Если все ок - применяем ход к реальной доске
    apply_move(m, board);
    return 0;
}

void apply_move(Move m, uint64_t *board) {
    // Получаем фигуру (целый байт с цветом и типом)
    Piece p = (board[m.src[0]] >> (m.src[1] << BYTEOFFSET_BITWIZE)) & 0xFF;
    Piece moved_piece = p;

    if (m.promotion) {
        moved_piece = (Piece)((p & COLOR_MASK) | m.promotion);
    }

    // Очищаем старое место
    board[m.src[0]] &= ~(0xFFULL << (m.src[1] << BYTEOFFSET_BITWIZE));

    if (m.en_passant) {
        board[m.dst[0]] &= ~(0xFFULL << (m.src[1] << BYTEOFFSET_BITWIZE));
    }

    // Очищаем целевое место (в случае взятия)
    board[m.dst[0]] &= ~(0xFFULL << (m.dst[1] << BYTEOFFSET_BITWIZE));

    // Ставим на новое место
    board[m.dst[0]] |= ((uint64_t)moved_piece << (m.dst[1] << BYTEOFFSET_BITWIZE));

    // Если это рокировка, передвигаем еще и ладью
    if (m.type == MOVE_SHORT_CASTLE || m.type == MOVE_LONG_CASTLE) {
        uint8_t row = m.src[1];
        uint8_t rook_src = (m.type == MOVE_SHORT_CASTLE) ? 7 : 0;
        uint8_t rook_dst = (m.type == MOVE_SHORT_CASTLE) ? 5 : 3;
        Piece rook = (board[rook_src] >> (row << BYTEOFFSET_BITWIZE)) & 0xFF;

        board[rook_src] &= ~(0xFFULL << (row << BYTEOFFSET_BITWIZE));
        board[rook_dst] &= ~(0xFFULL << (row << BYTEOFFSET_BITWIZE));
        board[rook_dst] |= ((uint64_t)rook << (row << BYTEOFFSET_BITWIZE));
    }
}
