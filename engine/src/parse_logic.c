#include <regex.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include "parse_logic.h"
#include "move_check.h"
#include "chess_table.h"
#include "move_logic.h"

regex_t regex;

#define MAX_GROUPS 16

static char parse_piece_char(char p_char) {
    switch (p_char) {
        case 'K': return KING;
        case 'Q': return QUEEN;
        case 'R': return ROOK;
        case 'B': return BISHOP;
        case 'N': return KNIGHT;
        default:  return PAWN;
    }
}

static void fill_suffix(Move *m, const char *suffix) {
    if (!suffix || !suffix[0]) return;

    if (strcmp(suffix, "e.p.") == 0) {
        m->en_passant = 1;
    } else if (suffix[0] == '+') {
        m->check = 1;
    } else if (suffix[0] == '#') {
        m->mate = 1;
    } else {
        m->promotion = parse_piece_char(suffix[0]);
    }
}

Move parse_string_to_move(const char* move_str) {
    Move m;
    m.src[0] = m.src[1] = 0;
    m.dst[0] = m.dst[1] = 0;
    m.piece = PAWN;
    m.promotion = 0;
    m.en_passant = 0;
    m.check = 0;
    m.mate = 0;
    m.type = MOVE_ERROR;
    regmatch_t groups[MAX_GROUPS];

    if (regexec(&regex, move_str, MAX_GROUPS, groups, 0) == 0) {
        if (groups[11].rm_so != -1) {
            m.type = MOVE_SHORT_CASTLE;
            m.piece = KING;
            if (groups[12].rm_so != -1) fill_suffix(&m, move_str + groups[12].rm_so);
            return m;
        }
        if (groups[9].rm_so != -1) {
            m.type = MOVE_LONG_CASTLE;
            m.piece = KING;
            if (groups[10].rm_so != -1) fill_suffix(&m, move_str + groups[10].rm_so);
            return m;
        }

        m.src[0] = move_str[groups[3].rm_so] - 'a';
        m.src[1] = move_str[groups[4].rm_so] - '1';
        m.dst[0] = move_str[groups[6].rm_so] - 'a';
        m.dst[1] = move_str[groups[7].rm_so] - '1';

        m.type = (groups[5].rm_so != -1 && groups[5].rm_so < groups[5].rm_eo &&
                  move_str[groups[5].rm_so] == 'x') ? MOVE_CAPTURE : MOVE_NORMAL;

        if (groups[2].rm_so != -1) {
            m.piece = parse_piece_char(move_str[groups[2].rm_so]);
        }

        if (groups[8].rm_so != -1) {
            char suffix[8];
            int len = groups[8].rm_eo - groups[8].rm_so;
            if (len > 0 && len < (int)sizeof(suffix)) {
                memcpy(suffix, move_str + groups[8].rm_so, (size_t)len);
                suffix[len] = '\0';
                fill_suffix(&m, suffix);
            }
        }
    }
    return m;
}
