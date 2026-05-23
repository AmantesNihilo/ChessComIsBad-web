#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <regex.h>

#include "chess_table.h"
#include "move_check.h"
#include "move_logic.h"
#include "parse_logic.h"
#include "game_state.h"

extern regex_t regex;

#define TEST_PASS 1
#define TEST_FAIL 0
#define TEST_SKIP 2

#define ASSERT_TRUE(value, message) do { \
    if (!(value)) { \
        printf("FAIL: %s: %s\n", __func__, message); \
        return TEST_FAIL; \
    } \
} while (0)

#define ASSERT_FALSE(value, message) do { \
    if (value) { \
        printf("FAIL: %s: %s\n", __func__, message); \
        return TEST_FAIL; \
    } \
} while (0)

#define ASSERT_EQ_INT(actual, expected, message) do { \
    int a = (int)(actual); \
    int e = (int)(expected); \
    if (a != e) { \
        printf("FAIL: %s: %s (expected %d, got %d)\n", __func__, message, e, a); \
        return TEST_FAIL; \
    } \
} while (0)

#define SKIP_TEST(message) do { \
    printf("SKIP: %s: %s\n", __func__, message); \
    return TEST_SKIP; \
} while (0)

typedef int (*TestFunc)(void);

static int g_tests_run = 0;
static int g_tests_failed = 0;
static int g_tests_skipped = 0;

static int init_test_regex(void) {
    const char *pat =
        "^(([KQRBN])?([a-h])([1-8])([x-]?)([a-h])([1-8])([KQRBN]|e\\.p\\.|\\+|#)?|(0-0-0|O-O-O)(\\+|#)?|(0-0|O-O)(\\+|#)?)$";
    return regcomp(&regex, pat, REG_EXTENDED);
}

static void board_clear(uint64_t *board) {
    for (int i = 0; i < 8; i++) board[i] = 0ULL;
}

static void board_set(uint64_t *board, uint8_t col, uint8_t row, Piece piece) {
    board[col] &= ~(0xFFULL << (row << BYTEOFFSET_BITWIZE));
    board[col] |= ((uint64_t)piece << (row << BYTEOFFSET_BITWIZE));
}

static Piece board_get(uint64_t *board, uint8_t col, uint8_t row) {
    return (Piece)((board[col] >> (row << BYTEOFFSET_BITWIZE)) & 0xFF);
}

static Move move_make(Piece piece, MoveType type, uint8_t src_col, uint8_t src_row,
                      uint8_t dst_col, uint8_t dst_row) {
    Move move;
    memset(&move, 0, sizeof(move));
    move.src[0] = src_col;
    move.src[1] = src_row;
    move.dst[0] = dst_col;
    move.dst[1] = dst_row;
    move.piece = (char)(piece & 0x3F);
    move.type = type;
    return move;
}

static int run_test(const char *name, TestFunc test) {
    int result;

    g_tests_run++;
    result = test();

    if (result == TEST_PASS) {
        printf("PASS: %s\n", name);
    } else if (result == TEST_SKIP) {
        g_tests_skipped++;
    } else {
        g_tests_failed++;
    }
    return result;
}

/* Pawn moves one square forward. */
static int test_pawn_move_one_forward(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 1};
    uint8_t dst[2] = {4, 2};
    board_clear(board);
    board_set(board, 4, 1, PAWN | WHITE);
    ASSERT_TRUE(correct_move_pawn(src, dst, board, 0), "pawn must move one square forward");
    return TEST_PASS;
}

/* Pawn moves two squares from start. */
static int test_pawn_first_double_move(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 1};
    uint8_t dst[2] = {4, 3};
    board_clear(board);
    board_set(board, 4, 1, PAWN | WHITE);
    ASSERT_TRUE(correct_move_pawn(src, dst, board, 0), "pawn must move two squares from start");
    return TEST_PASS;
}

/* Pawn cannot make second double move. */
static int test_pawn_second_double_move_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 2};
    uint8_t dst[2] = {4, 4};
    board_clear(board);
    board_set(board, 4, 2, PAWN | WHITE);
    ASSERT_FALSE(correct_move_pawn(src, dst, board, 0), "pawn must not double move after start");
    return TEST_PASS;
}

/* Pawn cannot move backward. */
static int test_pawn_backward_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 2};
    uint8_t dst[2] = {4, 1};
    board_clear(board);
    board_set(board, 4, 2, PAWN | WHITE);
    ASSERT_FALSE(correct_move_pawn(src, dst, board, 0), "pawn must not move backward");
    return TEST_PASS;
}

/* Pawn cannot move sideways without capture. */
static int test_pawn_sideways_without_capture_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 1};
    uint8_t dst[2] = {5, 1};
    board_clear(board);
    board_set(board, 4, 1, PAWN | WHITE);
    ASSERT_FALSE(correct_move_pawn(src, dst, board, 0), "pawn must not move sideways");
    return TEST_PASS;
}

/* Pawn cannot capture forward. */
static int test_pawn_capture_forward_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 1};
    uint8_t dst[2] = {4, 2};
    board_clear(board);
    board_set(board, 4, 1, PAWN | WHITE);
    board_set(board, 4, 2, KNIGHT | BLACK);
    ASSERT_FALSE(correct_move_pawn(src, dst, board, 0), "pawn must not capture forward");
    return TEST_PASS;
}

/* Pawn cannot capture backward. */
static int test_pawn_capture_backward_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 2};
    uint8_t dst[2] = {3, 1};
    board_clear(board);
    board_set(board, 4, 2, PAWN | WHITE);
    board_set(board, 3, 1, KNIGHT | BLACK);
    ASSERT_FALSE(correct_move_pawn(src, dst, board, 0), "pawn must not capture backward");
    return TEST_PASS;
}

/* Pawn cannot capture sideways. */
static int test_pawn_capture_sideways_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 1};
    uint8_t dst[2] = {5, 1};
    board_clear(board);
    board_set(board, 4, 1, PAWN | WHITE);
    board_set(board, 5, 1, KNIGHT | BLACK);
    ASSERT_FALSE(correct_move_pawn(src, dst, board, 0), "pawn must not capture sideways");
    return TEST_PASS;
}

/* Pawn captures diagonally. */
static int test_pawn_diagonal_capture_is_valid(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 1};
    uint8_t dst[2] = {5, 2};
    board_clear(board);
    board_set(board, 4, 1, PAWN | WHITE);
    board_set(board, 5, 2, KNIGHT | BLACK);
    ASSERT_TRUE(correct_move_pawn(src, dst, board, 0), "pawn must capture diagonally");
    return TEST_PASS;
}

/* Pinned pawn cannot open king line. */
static int test_pawn_pinned_capture_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 1};
    uint8_t dst[2] = {5, 2};
    board_clear(board);
    board_set(board, 4, 0, KING | WHITE);
    board_set(board, 4, 1, PAWN | WHITE);
    board_set(board, 4, 7, ROOK | BLACK);
    board_set(board, 5, 2, KNIGHT | BLACK);
    ASSERT_FALSE(correct_move_pawn(src, dst, board, 0), "pinned pawn must not move");
    return TEST_PASS;
}

/* Pawn cannot capture king. */
static int test_pawn_capture_king_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 1};
    uint8_t dst[2] = {5, 2};
    board_clear(board);
    board_set(board, 4, 1, PAWN | WHITE);
    board_set(board, 5, 2, KING | BLACK);
    ASSERT_FALSE(correct_move_pawn(src, dst, board, 0), "pawn must not capture king");
    return TEST_PASS;
}

/* Pawn cannot jump over piece on double move. */
static int test_pawn_double_move_blocked_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 1};
    uint8_t dst[2] = {4, 3};
    board_clear(board);
    board_set(board, 4, 1, PAWN | WHITE);
    board_set(board, 4, 2, KNIGHT | WHITE);
    ASSERT_FALSE(correct_move_pawn(src, dst, board, 0), "pawn must not jump over piece");
    return TEST_PASS;
}

/* Pawn cannot move into occupied square. */
static int test_pawn_forward_occupied_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 1};
    uint8_t dst[2] = {4, 2};
    board_clear(board);
    board_set(board, 4, 1, PAWN | WHITE);
    board_set(board, 4, 2, PAWN | BLACK);
    ASSERT_FALSE(correct_move_pawn(src, dst, board, 0), "pawn must not move into occupied square");
    return TEST_PASS;
}

/* Pawn cannot capture own piece. */
static int test_pawn_capture_own_piece_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 1};
    uint8_t dst[2] = {5, 2};
    board_clear(board);
    board_set(board, 4, 1, PAWN | WHITE);
    board_set(board, 5, 2, KNIGHT | WHITE);
    ASSERT_FALSE(correct_move_pawn(src, dst, board, 0), "pawn must not capture own piece");
    return TEST_PASS;
}

/* Pawn cannot move too far. */
static int test_pawn_long_move_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 1};
    uint8_t dst[2] = {4, 4};
    board_clear(board);
    board_set(board, 4, 1, PAWN | WHITE);
    ASSERT_FALSE(correct_move_pawn(src, dst, board, 0), "pawn must not move too far");
    return TEST_PASS;
}

/* Pawn cannot capture too far. */
static int test_pawn_long_capture_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 1};
    uint8_t dst[2] = {6, 3};
    board_clear(board);
    board_set(board, 4, 1, PAWN | WHITE);
    board_set(board, 6, 3, KNIGHT | BLACK);
    ASSERT_FALSE(correct_move_pawn(src, dst, board, 0), "pawn must not capture too far");
    return TEST_PASS;
}

/* Pawn can reach promotion rank. */
static int test_pawn_promotion_square_is_valid(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 6};
    uint8_t dst[2] = {4, 7};
    board_clear(board);
    board_set(board, 4, 6, PAWN | WHITE);
    ASSERT_TRUE(correct_move_pawn(src, dst, board, 0), "pawn must move to last rank");
    return TEST_PASS;
}

/* En passant helper accepts target square. */
static int test_pawn_en_passant_target_is_valid(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 4};
    uint8_t dst[2] = {5, 5};
    uint16_t en_passant_target = (uint16_t)(5 | (5 << 8));
    board_clear(board);
    board_set(board, 4, 4, PAWN | WHITE);
    ASSERT_TRUE(correct_move_pawn(src, dst, board, en_passant_target), "pawn must accept en passant target");
    return TEST_PASS;
}

/* Black pawn moves down. */
static int test_pawn_black_color_direction(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 6};
    uint8_t dst[2] = {4, 5};
    board_clear(board);
    board_set(board, 4, 6, PAWN | BLACK);
    ASSERT_TRUE(correct_move_pawn(src, dst, board, 0), "black pawn must move down");
    return TEST_PASS;
}

/* Turn order is not stored in move checker. */
static int test_pawn_same_color_twice_requires_game_state(void) {
    GameState game;
    game_init(&game);
    ASSERT_EQ_INT(game_execute_move(&game, "e2-e4"), 0, "white pawn move must pass");
    ASSERT_EQ_INT(game_execute_move(&game, "d2-d4"), -1, "white must not move twice");
    return TEST_PASS;
}

/* Rook moves vertically up. */
static int test_rook_vertical_up_is_valid(void) {
    uint64_t board[8];
    uint8_t src[2] = {0, 0};
    uint8_t dst[2] = {0, 7};
    board_clear(board);
    board_set(board, 0, 0, ROOK | WHITE);
    ASSERT_TRUE(correct_move_rook(src, dst, board), "rook must move up");
    return TEST_PASS;
}

/* Rook moves vertically down. */
static int test_rook_vertical_down_is_valid(void) {
    uint64_t board[8];
    uint8_t src[2] = {0, 7};
    uint8_t dst[2] = {0, 0};
    board_clear(board);
    board_set(board, 0, 7, ROOK | WHITE);
    ASSERT_TRUE(correct_move_rook(src, dst, board), "rook must move down");
    return TEST_PASS;
}

/* Rook moves horizontally. */
static int test_rook_horizontal_is_valid(void) {
    uint64_t board[8];
    uint8_t src[2] = {0, 0};
    uint8_t dst[2] = {7, 0};
    board_clear(board);
    board_set(board, 0, 0, ROOK | WHITE);
    ASSERT_TRUE(correct_move_rook(src, dst, board), "rook must move horizontally");
    return TEST_PASS;
}

/* Rook cannot move diagonally. */
static int test_rook_diagonal_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {0, 0};
    uint8_t dst[2] = {3, 3};
    board_clear(board);
    board_set(board, 0, 0, ROOK | WHITE);
    ASSERT_FALSE(correct_move_rook(src, dst, board), "rook must not move diagonally");
    return TEST_PASS;
}

/* Rook cannot jump over piece. */
static int test_rook_jump_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {0, 0};
    uint8_t dst[2] = {0, 7};
    board_clear(board);
    board_set(board, 0, 0, ROOK | WHITE);
    board_set(board, 0, 4, PAWN | WHITE);
    ASSERT_FALSE(correct_move_rook(src, dst, board), "rook must not jump");
    return TEST_PASS;
}

/* Rook cannot capture own piece. */
static int test_rook_capture_own_piece_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {0, 0};
    uint8_t dst[2] = {0, 7};
    board_clear(board);
    board_set(board, 0, 0, ROOK | WHITE);
    board_set(board, 0, 7, KNIGHT | WHITE);
    ASSERT_FALSE(correct_move_rook(src, dst, board), "rook must not capture own piece");
    return TEST_PASS;
}

/* Rook captures enemy piece. */
static int test_rook_capture_enemy_is_valid(void) {
    uint64_t board[8];
    uint8_t src[2] = {0, 0};
    uint8_t dst[2] = {0, 7};
    board_clear(board);
    board_set(board, 0, 0, ROOK | WHITE);
    board_set(board, 0, 7, KNIGHT | BLACK);
    ASSERT_TRUE(correct_move_rook(src, dst, board), "rook must capture enemy");
    return TEST_PASS;
}

/* Rook cannot capture through piece. */
static int test_rook_capture_through_piece_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {0, 0};
    uint8_t dst[2] = {0, 7};
    board_clear(board);
    board_set(board, 0, 0, ROOK | WHITE);
    board_set(board, 0, 3, PAWN | BLACK);
    board_set(board, 0, 7, KNIGHT | BLACK);
    ASSERT_FALSE(correct_move_rook(src, dst, board), "rook must not capture through piece");
    return TEST_PASS;
}

/* Rook cannot stay on same square. */
static int test_rook_same_square_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {0, 0};
    uint8_t dst[2] = {0, 0};
    board_clear(board);
    board_set(board, 0, 0, ROOK | WHITE);
    ASSERT_FALSE(correct_move_rook(src, dst, board), "rook must not stay on same square");
    return TEST_PASS;
}

/* Rook gives check on open file. */
static int test_rook_check_is_detected(void) {
    uint64_t board[8];
    uint8_t king_pos[2] = {4, 7};
    board_clear(board);
    board_set(board, 4, 7, KING | BLACK);
    board_set(board, 4, 0, ROOK | WHITE);
    ASSERT_TRUE(is_king_under_attack(king_pos, board), "rook check must be detected");
    return TEST_PASS;
}

/* Pinned rook cannot move away. */
static int test_rook_pinned_move_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 1};
    uint8_t dst[2] = {5, 1};
    Move move = move_make(ROOK, MOVE_NORMAL, 4, 1, 5, 1);
    board_clear(board);
    board_set(board, 4, 0, KING | WHITE);
    board_set(board, 4, 1, ROOK | WHITE);
    board_set(board, 4, 7, ROOK | BLACK);
    ASSERT_FALSE(is_valid(move, board), "pinned rook must not expose king");
    (void)src;
    (void)dst;
    return TEST_PASS;
}

/* Knight moves in all L directions. */
static int test_knight_all_l_moves_are_valid(void) {
    uint64_t board[8];
    uint8_t src[2] = {3, 3};
    uint8_t moves[8][2] = {{5, 4}, {5, 2}, {1, 4}, {1, 2}, {4, 5}, {4, 1}, {2, 5}, {2, 1}};
    board_clear(board);
    board_set(board, 3, 3, KNIGHT | WHITE);
    for (int i = 0; i < 8; i++) {
        ASSERT_TRUE(correct_move_knight(src, moves[i], board), "knight L move must be valid");
    }
    return TEST_PASS;
}

/* Knight cannot move not L. */
static int test_knight_non_l_move_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {3, 3};
    uint8_t dst[2] = {3, 5};
    board_clear(board);
    board_set(board, 3, 3, KNIGHT | WHITE);
    ASSERT_FALSE(correct_move_knight(src, dst, board), "knight must move L only");
    return TEST_PASS;
}

/* Knight can jump over pieces. */
static int test_knight_jump_is_valid(void) {
    uint64_t board[8];
    uint8_t src[2] = {1, 0};
    uint8_t dst[2] = {2, 2};
    board_clear(board);
    board_set(board, 1, 0, KNIGHT | WHITE);
    board_set(board, 1, 1, PAWN | WHITE);
    board_set(board, 2, 1, PAWN | WHITE);
    ASSERT_TRUE(correct_move_knight(src, dst, board), "knight must jump");
    return TEST_PASS;
}

/* Knight cannot capture own piece. */
static int test_knight_capture_own_piece_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {1, 0};
    uint8_t dst[2] = {2, 2};
    board_clear(board);
    board_set(board, 1, 0, KNIGHT | WHITE);
    board_set(board, 2, 2, PAWN | WHITE);
    ASSERT_FALSE(correct_move_knight(src, dst, board), "knight must not capture own piece");
    return TEST_PASS;
}

/* Knight captures enemy piece. */
static int test_knight_capture_enemy_is_valid(void) {
    uint64_t board[8];
    uint8_t src[2] = {1, 0};
    uint8_t dst[2] = {2, 2};
    board_clear(board);
    board_set(board, 1, 0, KNIGHT | WHITE);
    board_set(board, 2, 2, PAWN | BLACK);
    ASSERT_TRUE(correct_move_knight(src, dst, board), "knight must capture enemy");
    return TEST_PASS;
}

/* Knight cannot stay on same square. */
static int test_knight_same_square_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {1, 0};
    uint8_t dst[2] = {1, 0};
    board_clear(board);
    board_set(board, 1, 0, KNIGHT | WHITE);
    ASSERT_FALSE(correct_move_knight(src, dst, board), "knight must not stay");
    return TEST_PASS;
}

/* Knight cannot move too far. */
static int test_knight_too_long_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {1, 0};
    uint8_t dst[2] = {5, 5};
    board_clear(board);
    board_set(board, 1, 0, KNIGHT | WHITE);
    ASSERT_FALSE(correct_move_knight(src, dst, board), "knight must not move too far");
    return TEST_PASS;
}

/* Knight gives check. */
static int test_knight_check_is_detected(void) {
    uint64_t board[8];
    uint8_t king_pos[2] = {4, 4};
    board_clear(board);
    board_set(board, 4, 4, KING | BLACK);
    board_set(board, 2, 3, KNIGHT | WHITE);
    ASSERT_TRUE(is_king_under_attack(king_pos, board), "knight check must be detected");
    return TEST_PASS;
}

/* Knight cannot capture king. */
static int test_knight_capture_king_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {1, 0};
    uint8_t dst[2] = {2, 2};
    board_clear(board);
    board_set(board, 1, 0, KNIGHT | WHITE);
    board_set(board, 2, 2, KING | BLACK);
    ASSERT_FALSE(correct_move_knight(src, dst, board), "knight must not capture king");
    return TEST_PASS;
}

/* Bishop moves on all diagonals. */
static int test_bishop_all_diagonals_are_valid(void) {
    uint64_t board[8];
    uint8_t src[2] = {3, 3};
    uint8_t moves[4][2] = {{6, 6}, {0, 6}, {6, 0}, {0, 0}};
    board_clear(board);
    board_set(board, 3, 3, BISHOP | WHITE);
    for (int i = 0; i < 4; i++) {
        ASSERT_TRUE(correct_move_bishop(src, moves[i], board), "bishop diagonal must be valid");
    }
    return TEST_PASS;
}

/* Bishop cannot move vertically. */
static int test_bishop_vertical_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {3, 3};
    uint8_t dst[2] = {3, 6};
    board_clear(board);
    board_set(board, 3, 3, BISHOP | WHITE);
    ASSERT_FALSE(correct_move_bishop(src, dst, board), "bishop must not move vertically");
    return TEST_PASS;
}

/* Bishop cannot move horizontally. */
static int test_bishop_horizontal_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {3, 3};
    uint8_t dst[2] = {6, 3};
    board_clear(board);
    board_set(board, 3, 3, BISHOP | WHITE);
    ASSERT_FALSE(correct_move_bishop(src, dst, board), "bishop must not move horizontally");
    return TEST_PASS;
}

/* Bishop cannot jump. */
static int test_bishop_jump_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {2, 0};
    uint8_t dst[2] = {5, 3};
    board_clear(board);
    board_set(board, 2, 0, BISHOP | WHITE);
    board_set(board, 3, 1, PAWN | WHITE);
    ASSERT_FALSE(correct_move_bishop(src, dst, board), "bishop must not jump");
    return TEST_PASS;
}

/* Bishop cannot capture own piece. */
static int test_bishop_capture_own_piece_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {2, 0};
    uint8_t dst[2] = {5, 3};
    board_clear(board);
    board_set(board, 2, 0, BISHOP | WHITE);
    board_set(board, 5, 3, PAWN | WHITE);
    ASSERT_FALSE(correct_move_bishop(src, dst, board), "bishop must not capture own piece");
    return TEST_PASS;
}

/* Bishop captures enemy piece. */
static int test_bishop_capture_enemy_is_valid(void) {
    uint64_t board[8];
    uint8_t src[2] = {2, 0};
    uint8_t dst[2] = {5, 3};
    board_clear(board);
    board_set(board, 2, 0, BISHOP | WHITE);
    board_set(board, 5, 3, PAWN | BLACK);
    ASSERT_TRUE(correct_move_bishop(src, dst, board), "bishop must capture enemy");
    return TEST_PASS;
}

/* Bishop cannot capture through piece. */
static int test_bishop_capture_through_piece_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {2, 0};
    uint8_t dst[2] = {6, 4};
    board_clear(board);
    board_set(board, 2, 0, BISHOP | WHITE);
    board_set(board, 3, 1, PAWN | BLACK);
    board_set(board, 6, 4, KNIGHT | BLACK);
    ASSERT_FALSE(correct_move_bishop(src, dst, board), "bishop must not capture through piece");
    return TEST_PASS;
}

/* Bishop cannot stay on same square. */
static int test_bishop_same_square_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {2, 0};
    uint8_t dst[2] = {2, 0};
    board_clear(board);
    board_set(board, 2, 0, BISHOP | WHITE);
    ASSERT_FALSE(correct_move_bishop(src, dst, board), "bishop must not stay");
    return TEST_PASS;
}

/* Bishop gives check. */
static int test_bishop_check_is_detected(void) {
    uint64_t board[8];
    uint8_t king_pos[2] = {6, 4};
    board_clear(board);
    board_set(board, 6, 4, KING | BLACK);
    board_set(board, 2, 0, BISHOP | WHITE);
    ASSERT_TRUE(is_king_under_attack(king_pos, board), "bishop check must be detected");
    return TEST_PASS;
}

/* Bishop cannot capture king. */
static int test_bishop_capture_king_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {2, 0};
    uint8_t dst[2] = {5, 3};
    board_clear(board);
    board_set(board, 2, 0, BISHOP | WHITE);
    board_set(board, 5, 3, KING | BLACK);
    ASSERT_FALSE(correct_move_bishop(src, dst, board), "bishop must not capture king");
    return TEST_PASS;
}

/* Queen moves like rook. */
static int test_queen_rook_like_move_is_valid(void) {
    uint64_t board[8];
    uint8_t src[2] = {3, 3};
    uint8_t dst[2] = {3, 7};
    board_clear(board);
    board_set(board, 3, 3, QUEEN | WHITE);
    ASSERT_TRUE(correct_move_queen(src, dst, board), "queen must move like rook");
    return TEST_PASS;
}

/* Queen moves like bishop. */
static int test_queen_bishop_like_move_is_valid(void) {
    uint64_t board[8];
    uint8_t src[2] = {3, 3};
    uint8_t dst[2] = {6, 6};
    board_clear(board);
    board_set(board, 3, 3, QUEEN | WHITE);
    ASSERT_TRUE(correct_move_queen(src, dst, board), "queen must move like bishop");
    return TEST_PASS;
}

/* Queen cannot move on bad trajectory. */
static int test_queen_bad_trajectory_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {3, 3};
    uint8_t dst[2] = {5, 6};
    board_clear(board);
    board_set(board, 3, 3, QUEEN | WHITE);
    ASSERT_FALSE(correct_move_queen(src, dst, board), "queen must not move on bad trajectory");
    return TEST_PASS;
}

/* Queen cannot jump. */
static int test_queen_jump_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {3, 3};
    uint8_t dst[2] = {3, 7};
    board_clear(board);
    board_set(board, 3, 3, QUEEN | WHITE);
    board_set(board, 3, 5, PAWN | WHITE);
    ASSERT_FALSE(correct_move_queen(src, dst, board), "queen must not jump");
    return TEST_PASS;
}

/* Queen cannot capture own piece. */
static int test_queen_capture_own_piece_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {3, 3};
    uint8_t dst[2] = {3, 7};
    board_clear(board);
    board_set(board, 3, 3, QUEEN | WHITE);
    board_set(board, 3, 7, PAWN | WHITE);
    ASSERT_FALSE(correct_move_queen(src, dst, board), "queen must not capture own piece");
    return TEST_PASS;
}

/* Queen captures enemy piece. */
static int test_queen_capture_enemy_is_valid(void) {
    uint64_t board[8];
    uint8_t src[2] = {3, 3};
    uint8_t dst[2] = {3, 7};
    board_clear(board);
    board_set(board, 3, 3, QUEEN | WHITE);
    board_set(board, 3, 7, PAWN | BLACK);
    ASSERT_TRUE(correct_move_queen(src, dst, board), "queen must capture enemy");
    return TEST_PASS;
}

/* Queen cannot stay on same square. */
static int test_queen_same_square_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {3, 3};
    uint8_t dst[2] = {3, 3};
    board_clear(board);
    board_set(board, 3, 3, QUEEN | WHITE);
    ASSERT_FALSE(correct_move_queen(src, dst, board), "queen must not stay");
    return TEST_PASS;
}

/* Queen gives check. */
static int test_queen_check_is_detected(void) {
    uint64_t board[8];
    uint8_t king_pos[2] = {3, 7};
    board_clear(board);
    board_set(board, 3, 7, KING | BLACK);
    board_set(board, 3, 3, QUEEN | WHITE);
    ASSERT_TRUE(is_king_under_attack(king_pos, board), "queen check must be detected");
    return TEST_PASS;
}

/* Queen cannot capture king. */
static int test_queen_capture_king_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {3, 3};
    uint8_t dst[2] = {3, 7};
    board_clear(board);
    board_set(board, 3, 3, QUEEN | WHITE);
    board_set(board, 3, 7, KING | BLACK);
    ASSERT_FALSE(correct_move_queen(src, dst, board), "queen must not capture king");
    return TEST_PASS;
}

/* King moves one square in all directions. */
static int test_king_all_one_square_moves_are_valid(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 4};
    uint8_t moves[8][2] = {{4, 5}, {4, 3}, {5, 4}, {3, 4}, {5, 5}, {3, 5}, {5, 3}, {3, 3}};
    board_clear(board);
    board_set(board, 4, 4, KING | WHITE);
    for (int i = 0; i < 8; i++) {
        ASSERT_TRUE(correct_move_king(src, moves[i], board), "king one square move must be valid");
    }
    return TEST_PASS;
}

/* King cannot move more than one square. */
static int test_king_long_move_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 4};
    uint8_t dst[2] = {4, 6};
    board_clear(board);
    board_set(board, 4, 4, KING | WHITE);
    ASSERT_FALSE(correct_move_king(src, dst, board), "king must not move too far");
    return TEST_PASS;
}

/* King cannot move into check. */
static int test_king_move_under_check_is_invalid(void) {
    uint64_t board[8];
    Move move = move_make(KING, MOVE_NORMAL, 4, 0, 4, 1);
    board_clear(board);
    board_set(board, 4, 0, KING | WHITE);
    board_set(board, 4, 7, ROOK | BLACK);
    ASSERT_EQ_INT(execute_parsed_move(move, board), -2, "king must not move into check");
    return TEST_PASS;
}

/* King cannot capture own piece. */
static int test_king_capture_own_piece_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 4};
    uint8_t dst[2] = {5, 4};
    board_clear(board);
    board_set(board, 4, 4, KING | WHITE);
    board_set(board, 5, 4, PAWN | WHITE);
    ASSERT_FALSE(correct_move_king(src, dst, board), "king must not capture own piece");
    return TEST_PASS;
}

/* King captures enemy piece. */
static int test_king_capture_enemy_is_valid(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 4};
    uint8_t dst[2] = {5, 4};
    board_clear(board);
    board_set(board, 4, 4, KING | WHITE);
    board_set(board, 5, 4, PAWN | BLACK);
    ASSERT_TRUE(correct_move_king(src, dst, board), "king must capture enemy");
    return TEST_PASS;
}

/* King cannot stay on same square. */
static int test_king_same_square_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 4};
    uint8_t dst[2] = {4, 4};
    board_clear(board);
    board_set(board, 4, 4, KING | WHITE);
    ASSERT_FALSE(correct_move_king(src, dst, board), "king must not stay");
    return TEST_PASS;
}

/* Short castling is valid on clear board. */
static int test_king_short_castle_is_valid(void) {
    uint64_t board[8];
    Move move;
    board_clear(board);
    board_set(board, 4, 0, KING | WHITE);
    board_set(board, 7, 0, ROOK | WHITE);
    board_set(board, 7, 7, KING | BLACK);
    memset(&move, 0, sizeof(move));
    move.type = MOVE_SHORT_CASTLE;
    ASSERT_TRUE(prepare_castle_move(&move, WHITE, board), "short castling must be valid");
    ASSERT_EQ_INT(execute_parsed_move(move, board), 0, "short castling must execute");
    ASSERT_EQ_INT(board_get(board, 6, 0), KING | WHITE, "king must be on g1");
    ASSERT_EQ_INT(board_get(board, 5, 0), ROOK | WHITE, "rook must be on f1");
    return TEST_PASS;
}

/* Long castling is valid on clear board. */
static int test_king_long_castle_is_valid(void) {
    uint64_t board[8];
    Move move;
    board_clear(board);
    board_set(board, 4, 0, KING | WHITE);
    board_set(board, 0, 0, ROOK | WHITE);
    board_set(board, 7, 7, KING | BLACK);
    memset(&move, 0, sizeof(move));
    move.type = MOVE_LONG_CASTLE;
    ASSERT_TRUE(prepare_castle_move(&move, WHITE, board), "long castling must be valid");
    ASSERT_EQ_INT(execute_parsed_move(move, board), 0, "long castling must execute");
    ASSERT_EQ_INT(board_get(board, 2, 0), KING | WHITE, "king must be on c1");
    ASSERT_EQ_INT(board_get(board, 3, 0), ROOK | WHITE, "rook must be on d1");
    return TEST_PASS;
}

/* Castling through attacked square is invalid. */
static int test_king_castle_through_check_is_invalid(void) {
    uint64_t board[8];
    Move move;
    board_clear(board);
    board_set(board, 4, 0, KING | WHITE);
    board_set(board, 7, 0, ROOK | WHITE);
    board_set(board, 5, 7, ROOK | BLACK);
    move.type = MOVE_SHORT_CASTLE;
    ASSERT_FALSE(prepare_castle_move(&move, WHITE, board), "castling through check must be invalid");
    return TEST_PASS;
}

/* Castling while in check is invalid. */
static int test_king_castle_while_in_check_is_invalid(void) {
    uint64_t board[8];
    Move move;
    board_clear(board);
    board_set(board, 4, 0, KING | WHITE);
    board_set(board, 7, 0, ROOK | WHITE);
    board_set(board, 4, 7, ROOK | BLACK);
    move.type = MOVE_SHORT_CASTLE;
    ASSERT_FALSE(prepare_castle_move(&move, WHITE, board), "castling from check must be invalid");
    return TEST_PASS;
}

/* Castling after king movement needs history. */
static int test_king_castle_after_king_moved_requires_history(void) {
    GameState game;
    Move move;
    game_clear(&game);
    board_set(game.board, 4, 0, KING | WHITE);
    board_set(game.board, 7, 0, ROOK | WHITE);
    game.white_king_moved = 1;
    move.type = MOVE_SHORT_CASTLE;
    ASSERT_EQ_INT(game_execute_parsed_move(&game, move), -1, "castling after king move must fail");
    return TEST_PASS;
}

/* Castling after rook movement needs history. */
static int test_king_castle_after_rook_moved_requires_history(void) {
    GameState game;
    Move move;
    game_clear(&game);
    board_set(game.board, 4, 0, KING | WHITE);
    board_set(game.board, 7, 0, ROOK | WHITE);
    game.white_right_rook_moved = 1;
    move.type = MOVE_SHORT_CASTLE;
    ASSERT_EQ_INT(game_execute_parsed_move(&game, move), -1, "castling after rook move must fail");
    return TEST_PASS;
}

/* Mate detection needs game state. */
static int test_king_checkmate_requires_game_state(void) {
    GameState game;
    game_clear(&game);
    board_set(game.board, 7, 7, KING | BLACK);
    board_set(game.board, 6, 6, QUEEN | WHITE);
    board_set(game.board, 5, 5, KING | WHITE);
    ASSERT_TRUE(game_is_checkmate(&game, BLACK), "black must be checkmated");
    return TEST_PASS;
}

/* Stalemate detection needs game state. */
static int test_king_stalemate_requires_game_state(void) {
    GameState game;
    game_clear(&game);
    board_set(game.board, 7, 7, KING | BLACK);
    board_set(game.board, 6, 5, QUEEN | WHITE);
    board_set(game.board, 5, 6, KING | WHITE);
    ASSERT_TRUE(game_is_stalemate(&game, BLACK), "black must be stalemated");
    return TEST_PASS;
}

/* Double check detection needs game state. */
static int test_king_double_check_requires_game_state(void) {
    GameState game;
    game_clear(&game);
    board_set(game.board, 4, 7, KING | BLACK);
    board_set(game.board, 4, 0, ROOK | WHITE);
    board_set(game.board, 1, 4, BISHOP | WHITE);
    ASSERT_EQ_INT(game_count_attackers(&game, BLACK), 2, "black king must have two attackers");
    return TEST_PASS;
}

/* Move after checkmate is forbidden. */
static int test_game_move_after_checkmate_is_invalid(void) {
    GameState game;
    Move mate_move = move_make(QUEEN, MOVE_NORMAL, 6, 5, 6, 6);
    Move late_move = move_make(KING, MOVE_NORMAL, 7, 7, 6, 7);
    game_clear(&game);
    board_set(game.board, 7, 7, KING | BLACK);
    board_set(game.board, 6, 5, QUEEN | WHITE);
    board_set(game.board, 5, 5, KING | WHITE);
    ASSERT_EQ_INT(game_execute_parsed_move(&game, mate_move), 0, "mate move must execute");
    ASSERT_TRUE(game.game_over, "game must be over after mate");
    ASSERT_TRUE(game.checkmate, "checkmate flag must be set");
    ASSERT_EQ_INT(game_execute_parsed_move(&game, late_move), -4, "move after mate must fail");
    return TEST_PASS;
}

/* King cannot capture king. */
static int test_king_capture_king_is_invalid(void) {
    uint64_t board[8];
    uint8_t src[2] = {4, 4};
    uint8_t dst[2] = {5, 4};
    board_clear(board);
    board_set(board, 4, 4, KING | WHITE);
    board_set(board, 5, 4, KING | BLACK);
    ASSERT_FALSE(correct_move_king(src, dst, board), "king must not capture king");
    return TEST_PASS;
}

/* Kings cannot touch. */
static int test_king_touching_kings_are_invalid(void) {
    uint64_t board[8];
    Move move = move_make(KING, MOVE_NORMAL, 4, 0, 4, 1);
    board_clear(board);
    board_set(board, 4, 0, KING | WHITE);
    board_set(board, 4, 2, KING | BLACK);
    ASSERT_EQ_INT(execute_parsed_move(move, board), -2, "kings must not touch");
    return TEST_PASS;
}

/* Parser reads current project pawn notation. */
static int test_parse_pawn_with_dash_is_valid(void) {
    Move move = parse_string_to_move("e2-e4");
    ASSERT_EQ_INT(move.type, MOVE_NORMAL, "e2-e4 must parse");
    ASSERT_EQ_INT(move.piece, PAWN, "piece must be pawn");
    ASSERT_EQ_INT(move.src[0], 4, "source file must be e");
    ASSERT_EQ_INT(move.src[1], 1, "source rank must be 2");
    ASSERT_EQ_INT(move.dst[0], 4, "target file must be e");
    ASSERT_EQ_INT(move.dst[1], 3, "target rank must be 4");
    return TEST_PASS;
}

/* Parser reads current project knight notation. */
static int test_parse_knight_with_dash_is_valid(void) {
    Move move = parse_string_to_move("Ng1-f3");
    ASSERT_EQ_INT(move.type, MOVE_NORMAL, "Ng1-f3 must parse");
    ASSERT_EQ_INT(move.piece, KNIGHT, "piece must be knight");
    ASSERT_EQ_INT(move.src[0], 6, "source file must be g");
    ASSERT_EQ_INT(move.dst[0], 5, "target file must be f");
    return TEST_PASS;
}

/* Parser reads current project capture notation. */
static int test_parse_capture_is_valid(void) {
    Move move = parse_string_to_move("Qh5xf7");
    ASSERT_EQ_INT(move.type, MOVE_CAPTURE, "capture must parse");
    ASSERT_EQ_INT(move.piece, QUEEN, "piece must be queen");
    ASSERT_EQ_INT(move.dst[0], 5, "target file must be f");
    ASSERT_EQ_INT(move.dst[1], 6, "target rank must be 7");
    return TEST_PASS;
}

/* Parser should read compact pawn notation from spec. */
static int test_parse_compact_pawn_from_spec_is_valid(void) {
    Move move = parse_string_to_move("e2e4");
    ASSERT_EQ_INT(move.type, MOVE_NORMAL, "e2e4 must parse");
    return TEST_PASS;
}

/* Parser should read compact knight notation from spec. */
static int test_parse_compact_knight_from_spec_is_valid(void) {
    Move move = parse_string_to_move("Ng1f3");
    ASSERT_EQ_INT(move.type, MOVE_NORMAL, "Ng1f3 must parse");
    ASSERT_EQ_INT(move.piece, KNIGHT, "piece must be knight");
    return TEST_PASS;
}

/* Parser reads zero short castling notation. */
static int test_parse_zero_short_castle_is_valid(void) {
    Move move = parse_string_to_move("0-0");
    ASSERT_EQ_INT(move.type, MOVE_SHORT_CASTLE, "0-0 must parse");
    return TEST_PASS;
}

/* Parser reads zero long castling notation. */
static int test_parse_zero_long_castle_is_valid(void) {
    Move move = parse_string_to_move("0-0-0");
    ASSERT_EQ_INT(move.type, MOVE_LONG_CASTLE, "0-0-0 must parse");
    return TEST_PASS;
}

/* Parser should read letter short castling notation from spec. */
static int test_parse_letter_short_castle_from_spec_is_valid(void) {
    Move move = parse_string_to_move("O-O");
    ASSERT_EQ_INT(move.type, MOVE_SHORT_CASTLE, "O-O must parse");
    return TEST_PASS;
}

/* Parser should read letter long castling notation from spec. */
static int test_parse_letter_long_castle_from_spec_is_valid(void) {
    Move move = parse_string_to_move("O-O-O");
    ASSERT_EQ_INT(move.type, MOVE_LONG_CASTLE, "O-O-O must parse");
    return TEST_PASS;
}

/* Parser rejects empty string. */
static int test_parse_empty_string_is_invalid(void) {
    Move move = parse_string_to_move("");
    ASSERT_EQ_INT(move.type, MOVE_ERROR, "empty string must fail");
    return TEST_PASS;
}

/* Parser rejects too short string. */
static int test_parse_too_short_is_invalid(void) {
    Move move = parse_string_to_move("e2");
    ASSERT_EQ_INT(move.type, MOVE_ERROR, "too short string must fail");
    return TEST_PASS;
}

/* Parser rejects too long string. */
static int test_parse_too_long_is_invalid(void) {
    Move move = parse_string_to_move("e2-e4-extra");
    ASSERT_EQ_INT(move.type, MOVE_ERROR, "too long string must fail");
    return TEST_PASS;
}

/* Parser rejects bad symbols. */
static int test_parse_bad_symbols_are_invalid(void) {
    Move move = parse_string_to_move("@@@@");
    ASSERT_EQ_INT(move.type, MOVE_ERROR, "bad symbols must fail");
    return TEST_PASS;
}

/* Parser rejects bad coordinates. */
static int test_parse_bad_coordinates_are_invalid(void) {
    Move move = parse_string_to_move("e9-e4");
    ASSERT_EQ_INT(move.type, MOVE_ERROR, "bad coordinates must fail");
    return TEST_PASS;
}

/* Parser rejects same square move at execution level. */
static int test_parse_same_square_execution_is_invalid(void) {
    uint64_t board[8];
    Move move = parse_string_to_move("Ke1-e1");
    board_clear(board);
    board_set(board, 4, 0, KING | WHITE);
    ASSERT_EQ_INT(execute_parsed_move(move, board), -1, "same square move must fail");
    return TEST_PASS;
}

/* Parser rejects unknown piece. */
static int test_parse_unknown_piece_is_invalid(void) {
    Move move = parse_string_to_move("Xe2-e4");
    ASSERT_EQ_INT(move.type, MOVE_ERROR, "unknown piece must fail");
    return TEST_PASS;
}

/* Impossible parsed move must not execute. */
static int test_parse_impossible_move_execution_is_invalid(void) {
    uint64_t board[8];
    board_clear(board);
    init_chess_table(board);
    ASSERT_EQ_INT(execute_move("Bf1-e2", board), -1, "blocked bishop move must fail");
    return TEST_PASS;
}

int unit_tests_custom_main(void) {
    if (init_test_regex() != 0) {
        printf("Cannot init regex\n");
        return 2;
    }

    run_test("test_pawn_move_one_forward", test_pawn_move_one_forward);
    run_test("test_pawn_first_double_move", test_pawn_first_double_move);
    run_test("test_pawn_second_double_move_is_invalid", test_pawn_second_double_move_is_invalid);
    run_test("test_pawn_backward_is_invalid", test_pawn_backward_is_invalid);
    run_test("test_pawn_sideways_without_capture_is_invalid", test_pawn_sideways_without_capture_is_invalid);
    run_test("test_pawn_capture_forward_is_invalid", test_pawn_capture_forward_is_invalid);
    run_test("test_pawn_capture_backward_is_invalid", test_pawn_capture_backward_is_invalid);
    run_test("test_pawn_capture_sideways_is_invalid", test_pawn_capture_sideways_is_invalid);
    run_test("test_pawn_diagonal_capture_is_valid", test_pawn_diagonal_capture_is_valid);
    run_test("test_pawn_pinned_capture_is_invalid", test_pawn_pinned_capture_is_invalid);
    run_test("test_pawn_capture_king_is_invalid", test_pawn_capture_king_is_invalid);
    run_test("test_pawn_double_move_blocked_is_invalid", test_pawn_double_move_blocked_is_invalid);
    run_test("test_pawn_forward_occupied_is_invalid", test_pawn_forward_occupied_is_invalid);
    run_test("test_pawn_capture_own_piece_is_invalid", test_pawn_capture_own_piece_is_invalid);
    run_test("test_pawn_long_move_is_invalid", test_pawn_long_move_is_invalid);
    run_test("test_pawn_long_capture_is_invalid", test_pawn_long_capture_is_invalid);
    run_test("test_pawn_promotion_square_is_valid", test_pawn_promotion_square_is_valid);
    run_test("test_pawn_en_passant_target_is_valid", test_pawn_en_passant_target_is_valid);
    run_test("test_pawn_black_color_direction", test_pawn_black_color_direction);
    run_test("test_pawn_same_color_twice_requires_game_state", test_pawn_same_color_twice_requires_game_state);

    run_test("test_rook_vertical_up_is_valid", test_rook_vertical_up_is_valid);
    run_test("test_rook_vertical_down_is_valid", test_rook_vertical_down_is_valid);
    run_test("test_rook_horizontal_is_valid", test_rook_horizontal_is_valid);
    run_test("test_rook_diagonal_is_invalid", test_rook_diagonal_is_invalid);
    run_test("test_rook_jump_is_invalid", test_rook_jump_is_invalid);
    run_test("test_rook_capture_own_piece_is_invalid", test_rook_capture_own_piece_is_invalid);
    run_test("test_rook_capture_enemy_is_valid", test_rook_capture_enemy_is_valid);
    run_test("test_rook_capture_through_piece_is_invalid", test_rook_capture_through_piece_is_invalid);
    run_test("test_rook_same_square_is_invalid", test_rook_same_square_is_invalid);
    run_test("test_rook_check_is_detected", test_rook_check_is_detected);
    run_test("test_rook_pinned_move_is_invalid", test_rook_pinned_move_is_invalid);

    run_test("test_knight_all_l_moves_are_valid", test_knight_all_l_moves_are_valid);
    run_test("test_knight_non_l_move_is_invalid", test_knight_non_l_move_is_invalid);
    run_test("test_knight_jump_is_valid", test_knight_jump_is_valid);
    run_test("test_knight_capture_own_piece_is_invalid", test_knight_capture_own_piece_is_invalid);
    run_test("test_knight_capture_enemy_is_valid", test_knight_capture_enemy_is_valid);
    run_test("test_knight_same_square_is_invalid", test_knight_same_square_is_invalid);
    run_test("test_knight_too_long_is_invalid", test_knight_too_long_is_invalid);
    run_test("test_knight_check_is_detected", test_knight_check_is_detected);
    run_test("test_knight_capture_king_is_invalid", test_knight_capture_king_is_invalid);

    run_test("test_bishop_all_diagonals_are_valid", test_bishop_all_diagonals_are_valid);
    run_test("test_bishop_vertical_is_invalid", test_bishop_vertical_is_invalid);
    run_test("test_bishop_horizontal_is_invalid", test_bishop_horizontal_is_invalid);
    run_test("test_bishop_jump_is_invalid", test_bishop_jump_is_invalid);
    run_test("test_bishop_capture_own_piece_is_invalid", test_bishop_capture_own_piece_is_invalid);
    run_test("test_bishop_capture_enemy_is_valid", test_bishop_capture_enemy_is_valid);
    run_test("test_bishop_capture_through_piece_is_invalid", test_bishop_capture_through_piece_is_invalid);
    run_test("test_bishop_same_square_is_invalid", test_bishop_same_square_is_invalid);
    run_test("test_bishop_check_is_detected", test_bishop_check_is_detected);
    run_test("test_bishop_capture_king_is_invalid", test_bishop_capture_king_is_invalid);

    run_test("test_queen_rook_like_move_is_valid", test_queen_rook_like_move_is_valid);
    run_test("test_queen_bishop_like_move_is_valid", test_queen_bishop_like_move_is_valid);
    run_test("test_queen_bad_trajectory_is_invalid", test_queen_bad_trajectory_is_invalid);
    run_test("test_queen_jump_is_invalid", test_queen_jump_is_invalid);
    run_test("test_queen_capture_own_piece_is_invalid", test_queen_capture_own_piece_is_invalid);
    run_test("test_queen_capture_enemy_is_valid", test_queen_capture_enemy_is_valid);
    run_test("test_queen_same_square_is_invalid", test_queen_same_square_is_invalid);
    run_test("test_queen_check_is_detected", test_queen_check_is_detected);
    run_test("test_queen_capture_king_is_invalid", test_queen_capture_king_is_invalid);

    run_test("test_king_all_one_square_moves_are_valid", test_king_all_one_square_moves_are_valid);
    run_test("test_king_long_move_is_invalid", test_king_long_move_is_invalid);
    run_test("test_king_move_under_check_is_invalid", test_king_move_under_check_is_invalid);
    run_test("test_king_capture_own_piece_is_invalid", test_king_capture_own_piece_is_invalid);
    run_test("test_king_capture_enemy_is_valid", test_king_capture_enemy_is_valid);
    run_test("test_king_same_square_is_invalid", test_king_same_square_is_invalid);
    run_test("test_king_short_castle_is_valid", test_king_short_castle_is_valid);
    run_test("test_king_long_castle_is_valid", test_king_long_castle_is_valid);
    run_test("test_king_castle_through_check_is_invalid", test_king_castle_through_check_is_invalid);
    run_test("test_king_castle_while_in_check_is_invalid", test_king_castle_while_in_check_is_invalid);
    run_test("test_king_castle_after_king_moved_requires_history", test_king_castle_after_king_moved_requires_history);
    run_test("test_king_castle_after_rook_moved_requires_history", test_king_castle_after_rook_moved_requires_history);
    run_test("test_king_checkmate_requires_game_state", test_king_checkmate_requires_game_state);
    run_test("test_king_stalemate_requires_game_state", test_king_stalemate_requires_game_state);
    run_test("test_king_double_check_requires_game_state", test_king_double_check_requires_game_state);
    run_test("test_game_move_after_checkmate_is_invalid", test_game_move_after_checkmate_is_invalid);
    run_test("test_king_capture_king_is_invalid", test_king_capture_king_is_invalid);
    run_test("test_king_touching_kings_are_invalid", test_king_touching_kings_are_invalid);

    run_test("test_parse_pawn_with_dash_is_valid", test_parse_pawn_with_dash_is_valid);
    run_test("test_parse_knight_with_dash_is_valid", test_parse_knight_with_dash_is_valid);
    run_test("test_parse_capture_is_valid", test_parse_capture_is_valid);
    run_test("test_parse_compact_pawn_from_spec_is_valid", test_parse_compact_pawn_from_spec_is_valid);
    run_test("test_parse_compact_knight_from_spec_is_valid", test_parse_compact_knight_from_spec_is_valid);
    run_test("test_parse_zero_short_castle_is_valid", test_parse_zero_short_castle_is_valid);
    run_test("test_parse_zero_long_castle_is_valid", test_parse_zero_long_castle_is_valid);
    run_test("test_parse_letter_short_castle_from_spec_is_valid", test_parse_letter_short_castle_from_spec_is_valid);
    run_test("test_parse_letter_long_castle_from_spec_is_valid", test_parse_letter_long_castle_from_spec_is_valid);
    run_test("test_parse_empty_string_is_invalid", test_parse_empty_string_is_invalid);
    run_test("test_parse_too_short_is_invalid", test_parse_too_short_is_invalid);
    run_test("test_parse_too_long_is_invalid", test_parse_too_long_is_invalid);
    run_test("test_parse_bad_symbols_are_invalid", test_parse_bad_symbols_are_invalid);
    run_test("test_parse_bad_coordinates_are_invalid", test_parse_bad_coordinates_are_invalid);
    run_test("test_parse_same_square_execution_is_invalid", test_parse_same_square_execution_is_invalid);
    run_test("test_parse_unknown_piece_is_invalid", test_parse_unknown_piece_is_invalid);
    run_test("test_parse_impossible_move_execution_is_invalid", test_parse_impossible_move_execution_is_invalid);

    regfree(&regex);

    printf("\nTests: %d, failed: %d, skipped: %d\n",
           g_tests_run, g_tests_failed, g_tests_skipped);

    return (g_tests_failed == 0) ? 0 : 1;
}
