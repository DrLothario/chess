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
#include <algorithm>
#include <cstring>
#include "movesort.h"

void History::clear()
{
	memset(h, 0, sizeof(h));
}

int History::get(const Board& B, move_t m) const
{
	const int piece = B.get_piece_on(m.fsq()), tsq = m.tsq();
	assert(!move_is_cop(B, m) && piece_ok(piece));	
	assert(std::abs(h[piece][tsq]) < History::Max);
	
	return h[piece][tsq];
}

void History::add(const Board& B, move_t m, int bonus)
{
	const int piece = B.get_piece_on(m.fsq()), tsq = m.tsq();
	assert(!move_is_cop(B, m) && piece_ok(piece));
	h[piece][tsq] += bonus;
	
	if (std::abs(h[piece][tsq]) >= History::Max)
		for (int p = PAWN; p <= KING; ++p)
			for (int s = A1; s <= H8; h[p][s++] /= 2);
}

MoveSort::MoveSort(const Board* _B, GenType _type, const move_t *_killer, move_t _tt_move, const History *_H)
	: B(_B), type(_type), killer(_killer), tt_move(_tt_move), H(_H), idx(0)
{
	assert(type == ALL || type == CAPTURES_CHECKS || type == CAPTURES);

	/* If we're in check set type = ALL. This affects the sort() and uses SEE instead of MVV/LVA for
	 * example. It improves the quality of sorting for check evasions in the qsearch. */
	if (B->is_check())
		type = ALL;

	move_t mlist[MAX_MOVES];
	count = generate(type, mlist) - mlist;
	annotate(mlist);
}

move_t *MoveSort::generate(GenType type, move_t *mlist)
{
	if (type == ALL)
		return gen_moves(*B, mlist);
	else {
		// If we are in check, then type must be ALL (see constructor)
		assert(!B->is_check());

		move_t *end = mlist;
		Bitboard enemies = B->get_pieces(opp_color(B->get_turn()));

		end = gen_piece_moves(*B, enemies, end, true);
		end = gen_pawn_moves(*B, enemies | B->st().epsq_bb() | PPromotionRank[B->get_turn()], end, false);

		if (type == CAPTURES_CHECKS)
			end = gen_quiet_checks(*B, end);

		return end;
	}
}

void MoveSort::annotate(const move_t *mlist)
{
	for (int i = idx; i < count; ++i) {
		list[i].m = mlist[i];
		list[i].score = score(list[i].m);
	}
}

int MoveSort::score(move_t m)
{
	if (m == tt_move)
		return INF;
	else if (move_is_cop(*B, m))
		if (type == ALL) {
			/* equal and winning captures, by SEE, in front of quiet moves
			 * losing captures, after all quiet moves */
			int s = see(*B, m);			
			return s >= 0 ? s + History::Max : s - History::Max;
		} else
			return mvv_lva(*B, m);
	else {
		/* killers first, then the rest by history */
		if (killer && m == killer[0])
			return History::Max-1;
		else if (killer && m == killer[1])
			return History::Max-2;
		else
			return H->get(*B, m);
	}
}

move_t *MoveSort::next()
{
	if (idx < count) {
		std::swap(list[idx], *std::max_element(&list[idx], &list[count]));
		return &list[idx++].m;
	} else
		return NULL;
}

move_t *MoveSort::previous()
{
	if (idx > 0)
		return &list[--idx].m;
	else
		return NULL;
}