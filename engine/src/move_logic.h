#ifndef MOVE_LOGIC_H
#define MOVE_LOGIC_H

#include "move_check.h"
#include "parse_logic.h"

void apply_move(Move m, uint64_t *board);
int execute_parsed_move(Move m, uint64_t *board);

#endif
