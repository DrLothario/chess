/*
 * DiscoCheck, an UCI chess interface. Copyright (C) 2012 Lucas Braesch.
 *
 * DiscoCheck is free software: you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * DiscoCheck is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If not,
 * see <http://www.gnu.org/licenses/>.
*/
#pragma once
#include "movegen.h"
#include "tt.h"

struct SearchLimits {
	SearchLimits(): time(0), inc(0), movetime(0), depth(0), movestogo(0), nodes(0) {}
	int time, inc, movetime, depth, movestogo;
	uint64_t nodes;
};

// Transposition Table
extern TTable TT;

move_t bestmove(Board& B, const SearchLimits& sl);
void bench(int depth);