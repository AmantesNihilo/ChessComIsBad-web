#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chess_table.h"
#include "game_state.h"
#include "parse_logic.h"

#define LINE_SIZE 4096
#define MAX_LEGAL_MOVES 256
#define INF 100000000
#define MATE_SCORE 100000

extern regex_t regex;

typedef struct {
    Move move;
    char promotion;
} UciMove;

static int init_regex(void) {
    const char *pat =
        "^(([KQRBN])?([a-h])([1-8])([x-]?)([a-h])([1-8])|(0-0-0|O-O-O)|(0-0|O-O))$";
    return regcomp(&regex, pat, REG_EXTENDED);
}

static Piece piece_at(const GameState *game, int file, int rank) {
    return (Piece)((game->board[file] >> (rank << 3)) & 0xFF);
}

static void set_piece(GameState *game, int file, int rank, Piece piece) {
    game->board[file] &= ~(0xFFULL << (rank << 3));
    game->board[file] |= ((uint64_t)piece << (rank << 3));
}

static int is_coord(const char *s) {
    return s[0] >= 'a' && s[0] <= 'h' && s[1] >= '1' && s[1] <= '8' &&
           s[2] >= 'a' && s[2] <= 'h' && s[3] >= '1' && s[3] <= '8';
}

static char promotion_to_piece(char c) {
    switch ((char)tolower((unsigned char)c)) {
        case 'q': return QUEEN;
        case 'r': return ROOK;
        case 'b': return BISHOP;
        case 'n': return KNIGHT;
        default:  return 0;
    }
}

static Piece fen_to_piece(char c) {
    uint8_t color = isupper((unsigned char)c) ? WHITE : BLACK;

    switch ((char)tolower((unsigned char)c)) {
        case 'p': return (Piece)(color | PAWN);
        case 'n': return (Piece)(color | KNIGHT);
        case 'b': return (Piece)(color | BISHOP);
        case 'r': return (Piece)(color | ROOK);
        case 'q': return (Piece)(color | QUEEN);
        case 'k': return (Piece)(color | KING);
        default:  return EMPTY;
    }
}

static int load_fen(GameState *game, const char *placement, const char *turn,
                    const char *castling) {
    int file = 0;
    int rank = 7;

    game_clear(game);

    for (const char *p = placement; *p; p++) {
        if (*p == '/') {
            if (file != 8 || rank == 0) return 0;
            file = 0;
            rank--;
        } else if (isdigit((unsigned char)*p)) {
            file += *p - '0';
            if (file > 8) return 0;
        } else {
            Piece piece = fen_to_piece(*p);
            if (piece == EMPTY || file >= 8) return 0;
            set_piece(game, file, rank, piece);
            file++;
        }
    }

    if (rank != 0 || file != 8) return 0;

    if (strcmp(turn, "w") == 0) {
        game->current_color = WHITE;
    } else if (strcmp(turn, "b") == 0) {
        game->current_color = BLACK;
    } else {
        return 0;
    }

    game->white_king_moved = strchr(castling, 'K') || strchr(castling, 'Q') ? 0 : 1;
    game->black_king_moved = strchr(castling, 'k') || strchr(castling, 'q') ? 0 : 1;
    game->white_right_rook_moved = strchr(castling, 'K') ? 0 : 1;
    game->white_left_rook_moved = strchr(castling, 'Q') ? 0 : 1;
    game->black_right_rook_moved = strchr(castling, 'k') ? 0 : 1;
    game->black_left_rook_moved = strchr(castling, 'q') ? 0 : 1;

    return 1;
}

static int parse_uci_move(const GameState *game, const char *text, UciMove *out) {
    size_t len = strlen(text);
    if ((len != 4 && len != 5) || !is_coord(text)) return 0;

    int src_file = text[0] - 'a';
    int src_rank = text[1] - '1';
    int dst_file = text[2] - 'a';
    int dst_rank = text[3] - '1';
    Piece source = piece_at(game, src_file, src_rank);
    Piece target = piece_at(game, dst_file, dst_rank);
    char promotion = 0;

    if (source == EMPTY) return 0;

    if (len == 5) {
        promotion = promotion_to_piece(text[4]);
        if (!promotion || (source & 0x3F) != PAWN) return 0;
    }

    memset(out, 0, sizeof(*out));
    out->move.src[0] = (uint8_t)src_file;
    out->move.src[1] = (uint8_t)src_rank;
    out->move.dst[0] = (uint8_t)dst_file;
    out->move.dst[1] = (uint8_t)dst_rank;
    out->move.piece = (char)(source & 0x3F);
    out->move.type = (target == EMPTY) ? MOVE_NORMAL : MOVE_CAPTURE;
    out->promotion = promotion;

    if ((source & 0x3F) == KING && src_file == 4 && abs(dst_file - src_file) == 2) {
        out->move.type = (dst_file == 6) ? MOVE_SHORT_CASTLE : MOVE_LONG_CASTLE;
    }

    return 1;
}

static int execute_uci_move(GameState *game, const char *text) {
    UciMove uci_move;
    int ret;

    if (!parse_uci_move(game, text, &uci_move)) return -1;
    ret = game_execute_parsed_move(game, uci_move.move);
    if (ret == 0 && uci_move.promotion) {
        Piece moved = piece_at(game, uci_move.move.dst[0], uci_move.move.dst[1]);
        set_piece(game, uci_move.move.dst[0], uci_move.move.dst[1],
                  (Piece)((moved & COLOR_MASK) | uci_move.promotion));
    }
    return ret;
}

static int execute_candidate(GameState *game, UciMove candidate) {
    int ret = game_execute_parsed_move(game, candidate.move);
    if (ret == 0 && candidate.promotion) {
        Piece moved = piece_at(game, candidate.move.dst[0], candidate.move.dst[1]);
        set_piece(game, candidate.move.dst[0], candidate.move.dst[1],
                  (Piece)((moved & COLOR_MASK) | candidate.promotion));
    }
    return ret;
}

static void move_to_uci(const UciMove *uci_move, char out[6]) {
    out[0] = (char)('a' + uci_move->move.src[0]);
    out[1] = (char)('1' + uci_move->move.src[1]);
    out[2] = (char)('a' + uci_move->move.dst[0]);
    out[3] = (char)('1' + uci_move->move.dst[1]);
    if (uci_move->promotion) {
        char p = 'q';
        if (uci_move->promotion == ROOK) p = 'r';
        if (uci_move->promotion == BISHOP) p = 'b';
        if (uci_move->promotion == KNIGHT) p = 'n';
        out[4] = p;
        out[5] = '\0';
    } else {
        out[4] = '\0';
    }
}

static int try_candidate(const GameState *game, UciMove candidate) {
    GameState copy = *game;
    int ret = execute_candidate(&copy, candidate);
    return ret == 0;
}

static int generate_legal_moves(const GameState *game, UciMove moves[MAX_LEGAL_MOVES]) {
    int count = 0;

    for (int src_file = 0; src_file < 8; src_file++) {
        for (int src_rank = 0; src_rank < 8; src_rank++) {
            Piece source = piece_at(game, src_file, src_rank);
            if (source == EMPTY || (source & COLOR_MASK) != game->current_color) continue;

            for (int dst_file = 0; dst_file < 8; dst_file++) {
                for (int dst_rank = 0; dst_rank < 8; dst_rank++) {
                    Piece target = piece_at(game, dst_file, dst_rank);
                    UciMove candidate;

                    if (src_file == dst_file && src_rank == dst_rank) continue;
                    if (target != EMPTY && (target & COLOR_MASK) == game->current_color) continue;

                    memset(&candidate, 0, sizeof(candidate));
                    candidate.move.src[0] = (uint8_t)src_file;
                    candidate.move.src[1] = (uint8_t)src_rank;
                    candidate.move.dst[0] = (uint8_t)dst_file;
                    candidate.move.dst[1] = (uint8_t)dst_rank;
                    candidate.move.piece = (char)(source & 0x3F);
                    candidate.move.type = (target == EMPTY) ? MOVE_NORMAL : MOVE_CAPTURE;

                    if ((source & 0x3F) == KING && src_file == 4 &&
                        src_rank == dst_rank && abs(dst_file - src_file) == 2) {
                        candidate.move.type = (dst_file == 6) ? MOVE_SHORT_CASTLE : MOVE_LONG_CASTLE;
                    }

                    if ((source & 0x3F) == PAWN && (dst_rank == 0 || dst_rank == 7)) {
                        candidate.promotion = QUEEN;
                    }

                    if (try_candidate(game, candidate)) {
                        if (count < MAX_LEGAL_MOVES) {
                            moves[count++] = candidate;
                        }
                    }
                }
            }
        }
    }

    return count;
}

static int piece_value(Piece piece) {
    switch (piece & 0x3F) {
        case PAWN:   return 100;
        case KNIGHT: return 320;
        case BISHOP: return 330;
        case ROOK:   return 500;
        case QUEEN:  return 900;
        default:     return 0;
    }
}

static int evaluate(const GameState *game) {
    int score = 0;

    if (game->game_over && game->checkmate) {
        return game->current_color == WHITE ? -MATE_SCORE : MATE_SCORE;
    }
    if (game->game_over && game->stalemate) {
        return 0;
    }

    for (int file = 0; file < 8; file++) {
        for (int rank = 0; rank < 8; rank++) {
            Piece piece = piece_at(game, file, rank);
            if (piece == EMPTY) continue;

            int value = piece_value(piece);
            
            // Позиционный бонус: +10 сантипешек за контроль центральных полей
            int center_bonus = 0;
            if (file >= 2 && file <= 5 && rank >= 2 && rank <= 5) {
                // Если пешка или фигура ближе к полному центру (d4, d5, e4, e5), даем чуть больше
                if ((file == 3 || file == 4) && (rank == 3 || rank == 4)) {
                    center_bonus = 20; 
                } else {
                    center_bonus = 10;
                }
            }

            if ((piece & COLOR_MASK) == WHITE) {
                score += (value + center_bonus);
            }
            if ((piece & COLOR_MASK) == BLACK) {
                score -= (value + center_bonus);
            }
        }
    }

    return score;
}

static int alpha_beta(GameState *game, int depth, int alpha, int beta) {
    UciMove moves[MAX_LEGAL_MOVES];
    int count;

    if (depth <= 0 || game->game_over) {
        return evaluate(game);
    }

    count = generate_legal_moves(game, moves);
    if (count == 0) {
        return evaluate(game);
    }

    if (game->current_color == WHITE) {
        int best = -INF;

        for (int i = 0; i < count; i++) {
            GameState copy = *game;
            int score;

            if (execute_candidate(&copy, moves[i]) != 0) continue;
            score = alpha_beta(&copy, depth - 1, alpha, beta);

            if (score > best) best = score;
            if (score > alpha) alpha = score;
            if (alpha >= beta) break;
        }

        return best;
    } else {
        int best = INF;

        for (int i = 0; i < count; i++) {
            GameState copy = *game;
            int score;

            if (execute_candidate(&copy, moves[i]) != 0) continue;
            score = alpha_beta(&copy, depth - 1, alpha, beta);

            if (score < best) best = score;
            if (score < beta) beta = score;
            if (alpha >= beta) break;
        }

        return best;
    }
}
static int find_best_move(GameState *game, int depth, UciMove *best_move, int *best_score) {
    UciMove moves[MAX_LEGAL_MOVES];
    int count = generate_legal_moves(game, moves);

    if (count == 0) return 0;

    if (game->current_color == WHITE) {
        int best = -INF;
        int alpha = -INF; // Инициализируем локальную альфу

        for (int i = 0; i < count; i++) {
            GameState copy = *game;
            int score;

            if (execute_candidate(&copy, moves[i]) != 0) continue;
            
            // Передаем актуальные alpha и INF
            score = alpha_beta(&copy, depth - 1, alpha, INF);

            if (score > best) {
                best = score;
                *best_move = moves[i];
            }
            if (score > alpha) {
                alpha = score; // Сужаем окно для следующих веток
            }
        }
        *best_score = best;
    } else {
        int best = INF;
        int beta = INF; // Инициализируем локальную бету

        for (int i = 0; i < count; i++) {
            GameState copy = *game;
            int score;

            if (execute_candidate(&copy, moves[i]) != 0) continue;
            
            // Передаем актуальные -INF и beta
            score = alpha_beta(&copy, depth - 1, -INF, beta);

            if (score < best) {
                best = score;
                *best_move = moves[i];
            }
            if (score < beta) {
                beta = score; // Сужаем окно для следующих веток
            }
        }
        *best_score = best;
    }

    return 1;
}

static int parse_go_depth(const char *line) {
    char copy[LINE_SIZE];
    char *token;
    int depth = 2;

    strncpy(copy, line, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    token = strtok(copy, " \t\r\n");
    while (token) {
        if (strcmp(token, "depth") == 0) {
            token = strtok(NULL, " \t\r\n");
            if (token) {
                depth = atoi(token);
                if (depth < 1) depth = 1;
                if (depth > 5) depth = 5;
            }
            break;
        }
        token = strtok(NULL, " \t\r\n");
    }

    return depth;
}

static void handle_position(GameState *game, char *line) {
    char *token = strtok(line, " \t\r\n");
    int saw_position = 0;

    while (token) {
        if (!saw_position) {
            if (strcmp(token, "position") != 0) return;
            saw_position = 1;
        } else if (strcmp(token, "startpos") == 0) {
            game_init(game);
        } else if (strcmp(token, "fen") == 0) {
            char *fen[6];
            int count = 0;

            while (count < 6 && (token = strtok(NULL, " \t\r\n"))) {
                if (strcmp(token, "moves") == 0) break;
                fen[count++] = token;
            }

            if (count < 4 || !load_fen(game, fen[0], fen[1], fen[2])) {
                fprintf(stderr, "info string invalid fen ignored\n");
                return;
            }

            if (token && strcmp(token, "moves") != 0) {
                token = strtok(NULL, " \t\r\n");
            }
            if (!token || strcmp(token, "moves") != 0) return;
            continue;
        } else if (strcmp(token, "moves") == 0) {
            while ((token = strtok(NULL, " \t\r\n"))) {
                if (execute_uci_move(game, token) != 0) {
                    fprintf(stderr, "info string illegal move ignored: %s\n", token);
                    return;
                }
            }
            return;
        }

        token = strtok(NULL, " \t\r\n");
    }
}

static void handle_go(GameState *game, const char *line) {
    UciMove best;
    char text[6];
    int depth = parse_go_depth(line);
    int score = 0;

    if (!find_best_move(game, depth, &best, &score)) {
        puts("bestmove 0000");
        fflush(stdout);
        return;
    }

    move_to_uci(&best, text);

    // 1. Корректируем знак оценки под стандарт UCI (относительно ходящего игрока)
    int uci_score = score;
    if (game->current_color == BLACK) {
        uci_score = -score; 
    }

    // 2. Обрабатываем вывод мата / cp по стандарту UCI
    // Если оценка близка к MATE_SCORE, переводим её в формат "mate X"
    if (uci_score >= (MATE_SCORE - 1000)) {
        // Движок ставит мат. Вычисляем примерное число ходов (каждый ply в alpha_beta уменьшает/увеличивает score)
        int plies_to_mate = MATE_SCORE - uci_score;
        int moves_to_mate = (plies_to_mate + 1) / 2;
        if (moves_to_mate <= 0) moves_to_mate = 1; // защита от 0
        printf("info depth %d score mate %d\n", depth, moves_to_mate);
    } 
    else if (uci_score <= (-MATE_SCORE + 1000)) {
        // Движку ставят мат.
        int plies_to_mate = uci_score + MATE_SCORE;
        int moves_to_mate = (plies_to_mate - 1) / 2; // будет отрицательным для обозначения проигрыша
        if (moves_to_mate >= 0) moves_to_mate = -1;
        printf("info depth %d score mate %d\n", depth, moves_to_mate);
    } 
    else {
        // Обычная позиционная оценка в сотых долях пешки
        printf("info depth %d score cp %d\n", depth, uci_score);
    }

    if (execute_candidate(game, best) != 0) {
        puts("bestmove 0000");
    } else {
        printf("bestmove %s\n", text);
    }
    fflush(stdout);
}

int main(void) {
    GameState game;
    char line[LINE_SIZE];

    if (init_regex() != 0) {
        fprintf(stderr, "info string regex init failed\n");
        return 1;
    }

    game_init(&game);
    setvbuf(stdout, NULL, _IOLBF, 0);

    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\r\n")] = '\0';

        if (strcmp(line, "uci") == 0) {
            puts("id name ChessComIsBad");
            puts("id author Jokes_on_C");
            puts("uciok");
            fflush(stdout);
        } else if (strcmp(line, "isready") == 0) {
            puts("readyok");
            fflush(stdout);
        } else if (strcmp(line, "ucinewgame") == 0) {
            game_init(&game);
        } else if (strncmp(line, "position", 8) == 0) {
            handle_position(&game, line);
        } else if (strncmp(line, "go", 2) == 0) {
            handle_go(&game, line);
        } else if (strcmp(line, "stop") == 0) {
            puts("bestmove 0000");
            fflush(stdout);
        } else if (strcmp(line, "quit") == 0) {
            break;
        }
    }

    regfree(&regex);
    return 0;
}
