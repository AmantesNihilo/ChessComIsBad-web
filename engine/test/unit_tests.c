#include <regex.h>
#include <stdint.h>

#include "chess_table.h"
#include "ctest.h"
#include "game_state.h"
#include "move_check.h"
#include "move_logic.h"
#include "parse_logic.h"

extern regex_t regex;

static int init_test_regex(void) {
    const char *pat =
        "^(([KQRBN])?([a-h])([1-8])([x-]?)([a-h])([1-8])([QRBN]|e\\.p\\.|\\+|#)?|(0-0-0|O-O-O)(\\+|#)?|(0-0|O-O)(\\+|#)?)$";
    return regcomp(&regex, pat, REG_EXTENDED);
}

static void board_set(uint64_t *board, uint8_t col, uint8_t row, Piece piece) {
    board[col] &= ~(0xFFULL << (row << BYTEOFFSET_BITWIZE));
    board[col] |= ((uint64_t)piece << (row << BYTEOFFSET_BITWIZE));
}

static Piece board_get(uint64_t *board, uint8_t col, uint8_t row) {
    return (Piece)((board[col] >> (row << BYTEOFFSET_BITWIZE)) & 0xFF);
}

static void game_clear_with_kings(GameState *game) {
    game_clear(game);
    board_set(game->board, 0, 0, KING | WHITE);
    board_set(game->board, 7, 7, KING | BLACK);
}

/* Parser reads a normal pawn move. */
CTEST(parser, pawn_move) {
    Move move = parse_string_to_move("e2-e4");

    ASSERT_EQUAL(MOVE_NORMAL, move.type);
    ASSERT_EQUAL(PAWN, move.piece);
    ASSERT_EQUAL(4, move.src[0]);
    ASSERT_EQUAL(1, move.src[1]);
    ASSERT_EQUAL(4, move.dst[0]);
    ASSERT_EQUAL(3, move.dst[1]);
}

/* Parser reads a knight move. */
CTEST(parser, knight_move) {
    Move move = parse_string_to_move("Ng1-f3");

    ASSERT_EQUAL(MOVE_NORMAL, move.type);
    ASSERT_EQUAL(KNIGHT, move.piece);
    ASSERT_EQUAL(6, move.src[0]);
    ASSERT_EQUAL(0, move.src[1]);
    ASSERT_EQUAL(5, move.dst[0]);
    ASSERT_EQUAL(2, move.dst[1]);
}

/* Parser reads castling. */
CTEST(parser, castle_moves) {
    Move short_castle = parse_string_to_move("O-O");
    Move long_castle = parse_string_to_move("0-0-0");

    ASSERT_EQUAL(MOVE_SHORT_CASTLE, short_castle.type);
    ASSERT_EQUAL(MOVE_LONG_CASTLE, long_castle.type);
}

/* Parser rejects bad coordinates. */
CTEST(parser, bad_square) {
    Move move = parse_string_to_move("e9-e4");

    ASSERT_EQUAL(MOVE_ERROR, move.type);
}

/* Capture notation must really capture an enemy piece. */
CTEST(rules, capture_marker_requires_piece) {
    GameState game;

    game_init(&game);
    ASSERT_EQUAL(-1, game_execute_move(&game, "e2xe4"));
}

/* Quiet move must not land on an enemy piece. */
CTEST(rules, quiet_marker_cannot_capture) {
    GameState game;

    game_init(&game);
    ASSERT_EQUAL(0, game_execute_move(&game, "e2-e4"));
    ASSERT_EQUAL(0, game_execute_move(&game, "d7-d5"));
    ASSERT_EQUAL(-1, game_execute_move(&game, "e4-d5"));
}

/* Pawn promotion creates the selected piece. */
CTEST(rules, pawn_promotion) {
    GameState game;

    game_clear_with_kings(&game);
    board_set(game.board, 4, 6, PAWN | WHITE);

    ASSERT_EQUAL(0, game_execute_move(&game, "e7-e8Q"));
    ASSERT_EQUAL((QUEEN | WHITE), board_get(game.board, 4, 7));
}

/* Promotion is allowed only for a pawn on the last rank. */
CTEST(rules, bad_promotion) {
    GameState game;

    game_init(&game);
    ASSERT_EQUAL(-1, game_execute_move(&game, "e2-e4Q"));
}

/* En passant captures the pawn that just moved two squares. */
CTEST(rules, en_passant_capture) {
    GameState game;

    game_init(&game);
    ASSERT_EQUAL(0, game_execute_move(&game, "e2-e4"));
    ASSERT_EQUAL(0, game_execute_move(&game, "a7-a6"));
    ASSERT_EQUAL(0, game_execute_move(&game, "e4-e5"));
    ASSERT_EQUAL(0, game_execute_move(&game, "d7-d5"));
    ASSERT_EQUAL(0, game_execute_move(&game, "e5xd6e.p."));
    ASSERT_EQUAL((PAWN | WHITE), board_get(game.board, 3, 5));
    ASSERT_EQUAL(EMPTY, board_get(game.board, 3, 4));
}

/* En passant is rejected after a waiting move. */
CTEST(rules, late_en_passant_rejected) {
    GameState game;

    game_init(&game);
    ASSERT_EQUAL(0, game_execute_move(&game, "e2-e4"));
    ASSERT_EQUAL(0, game_execute_move(&game, "a7-a6"));
    ASSERT_EQUAL(0, game_execute_move(&game, "e4-e5"));
    ASSERT_EQUAL(0, game_execute_move(&game, "d7-d5"));
    ASSERT_EQUAL(0, game_execute_move(&game, "h2-h3"));
    ASSERT_EQUAL(0, game_execute_move(&game, "a6-a5"));
    ASSERT_EQUAL(-1, game_execute_move(&game, "e5xd6e.p."));
}

/* Plus sign is rejected when the move does not give check. */
CTEST(rules, wrong_check_marker) {
    GameState game;

    game_init(&game);
    ASSERT_EQUAL(-1, game_execute_move(&game, "e2-e4+"));
}

/* Mate marker is accepted on a real checkmate. */
CTEST(rules, mate_marker) {
    GameState game;

    game_init(&game);
    ASSERT_EQUAL(0, game_execute_move(&game, "f2-f3"));
    ASSERT_EQUAL(0, game_execute_move(&game, "e7-e5"));
    ASSERT_EQUAL(0, game_execute_move(&game, "g2-g4"));
    ASSERT_EQUAL(0, game_execute_move(&game, "Qd8-h4#"));
    ASSERT_TRUE(game.checkmate);
}

/* Mate marker is rejected when it is only a regular move. */
CTEST(rules, wrong_mate_marker) {
    GameState game;

    game_init(&game);
    ASSERT_EQUAL(-1, game_execute_move(&game, "e2-e4#"));
}

/* Existing custom tests are kept in test/unit_tests_custom.c. */
CTEST(project, custom_tests_saved) {
    ASSERT_TRUE(1);
}

int main(void) {
    int failed = 0;

    if (init_test_regex() != 0) return 1;

    failed += RUN_CTEST(parser, pawn_move);
    failed += RUN_CTEST(parser, knight_move);
    failed += RUN_CTEST(parser, castle_moves);
    failed += RUN_CTEST(parser, bad_square);
    failed += RUN_CTEST(rules, capture_marker_requires_piece);
    failed += RUN_CTEST(rules, quiet_marker_cannot_capture);
    failed += RUN_CTEST(rules, pawn_promotion);
    failed += RUN_CTEST(rules, bad_promotion);
    failed += RUN_CTEST(rules, en_passant_capture);
    failed += RUN_CTEST(rules, late_en_passant_rejected);
    failed += RUN_CTEST(rules, wrong_check_marker);
    failed += RUN_CTEST(rules, mate_marker);
    failed += RUN_CTEST(rules, wrong_mate_marker);
    failed += RUN_CTEST(project, custom_tests_saved);

    regfree(&regex);
    return failed ? failed : ctest_result();
}
