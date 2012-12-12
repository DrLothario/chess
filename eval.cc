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
#include <cstring>
#include "eval.h"

int KingDistanceToSafety[NB_COLOR][NB_SQUARE];
int KingDistance[NB_SQUARE][NB_SQUARE];

int kdist(int s1, int s2)
{
	return KingDistance[s1][s2];
}

void init_eval()
{
	for (int s1 = A1; s1 <= H8; ++s1)
		for (int s2 = A1; s2 <= H8; ++s2)
			KingDistance[s1][s2] = std::max(std::abs(file(s1)-file(s2)), std::abs(rank(s1)-rank(s2)));

	for (int us = WHITE; us <= BLACK; ++us)
		for (int sq = A1; sq <= H8; ++sq)
			KingDistanceToSafety[us][sq] = std::min(kdist(sq, us ? E8 : E1), kdist(sq, us ? B8 : B1));
}

class PawnCache
{
public:
	struct Entry
	{
		Key key;
		Eval eval_white;
		Bitboard passers;
	};

	PawnCache() { memset(buf, 0, sizeof(buf)); }
	Entry &probe(Key key) { return buf[key & (count-1)]; }

private:
	static const int count = 0x10000;
	Entry buf[count];
};

PawnCache PC;

class EvalInfo
{
public:
	EvalInfo(const Board *_B): B(_B)
	{
		e[WHITE].clear();
		e[BLACK].clear();
	}

	void eval_material();
	void eval_mobility();
	void eval_safety();
	void eval_pieces();
	void eval_pawns();
	int interpolate() const;

private:
	const Board *B;
	Eval e[NB_COLOR];

	Bitboard do_eval_pawns();
	void eval_passer(int sq);

	int calc_phase() const;
	Eval eval_white() const
	{
		Eval tmp = e[WHITE];
		return tmp -= e[BLACK];
	}
};

void EvalInfo::eval_material()
{
	for (int color = WHITE; color <= BLACK; ++color)
	{
		// material (PSQ)
		e[color] += B->st().psq[color];

		// bishop pair
		if (several_bits(B->get_pieces(color, BISHOP)))
		{
			e[color].op += 40;
			e[color].eg += 50;
		}
	}

	// If the stronger side has no pawns, half the material difference in the endgame
	const int strong_side = e[BLACK].eg > e[WHITE].eg;
	if (!B->get_pieces(strong_side, PAWN))
		e[strong_side].eg -= std::abs(e[WHITE].eg - e[BLACK].eg)/2;
}

// Generic linear mobility
#define MOBILITY(p0, p)											\
	count = mob_count[p0][count_bit_max15(tss & mob_targets)];	\
	e[us].op += count * mob_unit[OPENING][p];					\
	e[us].eg += count * mob_unit[ENDGAME][p]

void EvalInfo::eval_mobility()
{
	static const int mob_count[ROOK+1][15] =
	{
		{},
		{-3, -2, -1, 0, 1, 2, 3, 4, 4},
		{-4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 5, 6, 6, 7},
		{-5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 6, 7, 7}
	};
	static const unsigned mob_unit[NB_PHASE][NB_PIECE] =
	{
		{0, 4, 5, 2, 1, 0},		// Opening
		{0, 4, 5, 4, 2, 0}		// EndGame
	};

	for (int color = WHITE; color <= BLACK; color++)
	{
		const int us = color, them = opp_color(us);
		const Bitboard mob_targets = ~(B->get_pieces(us, PAWN) | B->get_pieces(us, KING)
		                               | B->st().attacks[them][PAWN]);

		Bitboard fss, tss, occ;
		int fsq, piece, count;

		// Knight mobility
		fss = B->get_pieces(us, KNIGHT);
		while (fss)
		{
			tss = NAttacks[pop_lsb(&fss)];
			MOBILITY(KNIGHT, KNIGHT);
		}

		// Lateral mobility
		fss = B->get_RQ(us);
		occ = B->st().occ & ~B->get_pieces(us, ROOK);		// see through rooks
		while (fss)
		{
			fsq = pop_lsb(&fss);
			piece = B->get_piece_on(fsq);
			tss = rook_attack(fsq, occ);
			MOBILITY(ROOK, piece);
		}

		// Diagonal mobility
		fss = B->get_BQ(us);
		occ = B->st().occ & ~B->get_pieces(us, BISHOP);		// see through rooks
		while (fss)
		{
			fsq = pop_lsb(&fss);
			piece = B->get_piece_on(fsq);
			tss = bishop_attack(fsq, occ);
			MOBILITY(BISHOP, piece);
		}
	}
}

// Generic attack scoring
#define ADD_ATTACK(p0)								\
	if (sq_attackers) {								\
		count = count_bit(sq_attackers);			\
		total_weight += AttackWeight[p0] * count;	\
		if (test_bit(defended, sq)) count--;		\
		total_count += count;						\
	}

void EvalInfo::eval_safety()
{
	static const int AttackWeight[NB_PIECE] = {0, 3, 3, 4, 0, 0};

	for (int color = WHITE; color <= BLACK; color++)
	{
		const int us = color, them = opp_color(us), ksq = B->get_king_pos(us);
		const Bitboard their_pawns = B->get_pieces(them, PAWN);

		// Squares that defended by pawns or occupied by attacker pawns, are useless as far as piece
		// attacks are concerned
		const Bitboard solid = B->st().attacks[us][PAWN] | their_pawns;

		// Defended by our pieces
		const Bitboard defended = B->st().attacks[us][KNIGHT]
		                          | B->st().attacks[us][BISHOP]
		                          | B->st().attacks[us][ROOK];

		int total_weight = 0, total_count = 0, sq, count;
		Bitboard sq_attackers, attacked, occ, fss;

		// Knight attacks
		attacked = B->st().attacks[them][KNIGHT] & (KAttacks[ksq] | NAttacks[ksq]) & ~solid;
		if (attacked)
		{
			fss = B->get_pieces(them, KNIGHT);
			while (attacked)
			{
				sq = pop_lsb(&attacked);
				sq_attackers = NAttacks[sq] & fss;
				ADD_ATTACK(KNIGHT);
			}
		}

		// Lateral attacks
		attacked = B->st().attacks[them][ROOK] & KAttacks[ksq] & ~solid;
		if (attacked)
		{
			fss = B->get_RQ(them);
			occ = B->st().occ & ~fss;	// rooks and queens see through each other
			while (attacked)
			{
				sq = pop_lsb(&attacked);
				sq_attackers = fss & rook_attack(sq, occ);
				ADD_ATTACK(ROOK);
			}
		}

		// Diagonal attacks
		attacked = B->st().attacks[them][BISHOP] & KAttacks[ksq] & ~solid;
		if (attacked)
		{
			fss = B->get_BQ(them);
			occ = B->st().occ & ~fss;	// bishops and queens see through each other
			while (attacked)
			{
				sq = pop_lsb(&attacked);
				sq_attackers = fss & bishop_attack(sq, occ);
				ADD_ATTACK(BISHOP);
			}
		}

		// Adjust for king's "distance to safety"
		total_count += KingDistanceToSafety[us][ksq];

		if (total_count)
			e[us].op -= total_weight * total_count;
	}
}

void EvalInfo::eval_passer(int sq)
{
	const int us = B->get_color_on(sq), them = opp_color(us);

	if (!B->st().piece_psq[them])
	{
		// opponent has no pieces
		const int psq = square(us ? RANK_1 : RANK_8, file(sq));
		const int pd = kdist(sq, psq);
		const int kd = kdist(B->get_king_pos(them), psq) - (them == B->get_turn());

		if (kd > pd)  	// unstoppable passer
		{
			e[us].eg += vR;	// on top of the bonus from do_eval_pawns()
			return;
		}
	}

	const int r = rank(sq);
	const int L = (us ? 7-r : r) - RANK_2;	// Linear part		0..5
	const int Q = L*(L-1);					// Quadratic part	0..20

	if (Q && !test_bit(B->st().occ, pawn_push(us, sq)))
	{
		const Bitboard path = SquaresInFront[us][sq];
		const Bitboard b = file_bb(file(sq)) & rook_attack(sq, B->st().occ);

		uint64_t defended, attacked;
		if (B->get_RQ(them) & b)
		{
			defended = path & B->st().attacks[us][NO_PIECE];
			attacked = path;
		}
		else
		{
			defended = (B->get_RQ(us) & b) ? path : path & B->st().attacks[us][NO_PIECE];
			attacked = path & (B->st().attacks[them][NO_PIECE] | B->get_pieces(them));
		}

		if (!attacked)
			e[us].eg += Q * (path == defended ? 7 : 6);
		else
			e[us].eg += Q * ((attacked & defended) == attacked ? 5 : 3);
	}
}

void EvalInfo::eval_pawns()
{
	const Key key = B->st().kpkey;
	PawnCache::Entry h = PC.probe(key);

	if (h.key == key)
		e[WHITE] += h.eval_white;
	else
	{
		const Eval ew0 = eval_white();
		h.key = key;
		h.passers = do_eval_pawns();
		h.eval_white = eval_white();
		h.eval_white -= ew0;
	}

	// piece-dependant passed pawn scoring
	Bitboard b = h.passers;
	while (b)
		eval_passer(pop_lsb(&b));
}

Bitboard EvalInfo::do_eval_pawns()
{
	static const int Chained = 5, Isolated = 20;
	static const Eval Hole = {16, 10};
	static const int ShelterPenalty[8] = {55, 0, 15, 40, 50, 55, 55, 0};
	static const int StormPenalty[8] = {10, 0, 50, 20, 10, 0, 0, 0};

	Bitboard passers = 0;

	for (int color = WHITE; color <= BLACK; color++)
	{
		const int us = color, them = opp_color(us);
		const int our_ksq = B->get_king_pos(us), their_ksq = B->get_king_pos(them);
		const Bitboard our_pawns = B->get_pieces(us, PAWN), their_pawns = B->get_pieces(them, PAWN);
		Bitboard sqs = our_pawns;

		int kf = file(our_ksq);
		for (int f = kf-1; f <= kf+1; ++f)
		{
			if (f < FILE_A || f > FILE_H)
				continue;

			Bitboard b;
			int r, sq;
			bool half;

			// Pawn shelter
			b = our_pawns & file_bb(f);
			r = b ? (us ? 7-rank(msb(b)) : rank(lsb(b))): 0;
			half = f != kf;
			e[us].op -= ShelterPenalty[r] >> half;

			// Pawn storm
			b = their_pawns & file_bb(f);
			if (b)
			{
				sq = us ? msb(b) : lsb(b);
				r = us ? 7-rank(sq) : rank(sq);
				half = test_bit(our_pawns, pawn_push(them, sq));
			}
			else
			{
				r = RANK_1;		// actually we penalize for the semi open file here
				half = false;
			}
			e[us].op -= StormPenalty[r] >> half;
		}

		while (sqs)
		{
			const int sq = pop_lsb(&sqs), next_sq = pawn_push(us, sq);
			const int r = rank(sq), f = file(sq);
			const Bitboard besides = our_pawns & AdjacentFiles[f];

			const bool chained = besides & (rank_bb(r) | rank_bb(us ? r+1 : r-1));
			const bool hole = !chained && !(PawnSpan[them][next_sq] & our_pawns)
			                  && test_bit(B->st().attacks[them][PAWN], next_sq);
			const bool isolated = !besides;

			const bool open = !(SquaresInFront[us][sq] & (our_pawns | their_pawns));
			const bool passed = open && !(PawnSpan[us][sq] & their_pawns);
			const bool candidate = chained && open && !passed
			                       && !several_bits(PawnSpan[us][sq] & their_pawns);

			if (chained)
				e[us].op += Chained;
			else if (hole)
			{
				e[us].op -= open ? Hole.op : Hole.op/2;
				e[us].eg -= Hole.eg;
			}
			else if (isolated)
			{
				e[us].op -= open ? Isolated : Isolated/2;
				e[us].eg -= Isolated;
			}

			if (candidate)
			{
				int n = us ? 7-r : r;
				const int d1 = kdist(sq, our_ksq), d2 = kdist(sq, their_ksq);

				if (d1 > d2)		// penalise if enemy king is closer
					n -= d1 - d2;

				if (n > 0)			// quadratic score
					e[us].eg += n*n;
			}
			else if (passed)
			{
				set_bit(&passers, sq);

				const int L = (us ? RANK_8-r : r)-RANK_2;	// Linear part		0..5
				const int Q = L*(L-1);						// Quadratic part	0..20

				// score based on rank
				e[us].op += 8 * Q;
				e[us].eg += 4 * (Q + L + 1);

				if (Q)
				{
					//  adjustment for king distance
					e[us].eg += kdist(next_sq, their_ksq) * 2 * Q;
					e[us].eg -= kdist(next_sq, our_ksq) * Q;
					if (rank(next_sq) != (us ? RANK_1 : RANK_8))
						e[us].eg -= kdist(pawn_push(us, next_sq), our_ksq) * Q / 2;
				}

				// support by friendly pawn
				if (besides & PawnSpan[them][next_sq])
				{
					if (PAttacks[them][next_sq] & our_pawns)
						e[us].eg += 8 * L;	// besides is good, as it allows a further push
					else if (PAttacks[them][sq] & our_pawns)
						e[us].eg += 5 * L;	// behind is solid, but doesn't allow further push
					else if (!(their_pawns & PawnSpan[them][sq]))
						e[us].eg += 2 * L;	// further behind
				}
			}
		}
	}

	return passers;
}

void EvalInfo::eval_pieces()
{
	static const int Rook7th = 8;
	static const uint64_t BishopTrap[NB_COLOR] =
	{
		(1ULL << A7) | (1ULL << H7),
		(1ULL << A2) | (1ULL << H2)
	};
	static const Bitboard KnightTrap[NB_COLOR] =
	{
		(1ULL << A8) | (1ULL << H8) | (1ULL << A7) | (1ULL << H7),
		(1ULL << A1) | (1ULL << H1) | (1ULL << A2) | (1ULL << H2)
	};

	for (int color = WHITE; color <= BLACK; ++color)
	{
		const int us = color, them = opp_color(us);
		Bitboard fss, tss;

		// Rook or Queen on 7th rank
		fss = B->get_RQ(us);
		tss = PInitialRank[them];
		// If we have are rooks and/or queens on the 7th rank, and the enemy king is on the 8th, or
		// there are enemy pawns on the 7th
		if ((fss & tss) && ((PPromotionRank[us] & B->get_pieces(them, KING)) || (tss & B->get_pieces(them, PAWN))))
		{
			int count;

			// Rook(s) on 7th rank
			if ((count = count_bit(B->get_pieces(us, ROOK) & tss)))
			{
				e[us].op += count * Rook7th/2;
				e[us].eg += count * Rook7th;
			}

			// Queen(s) on 7th rank
			if ((count = count_bit(B->get_pieces(us, QUEEN) & tss)))
			{
				e[us].op += count * Rook7th/4;
				e[us].eg += count * Rook7th/2;
			}
		}

		// Knight trapped
		fss = B->get_pieces(us, KNIGHT) & KnightTrap[us];
		while (fss)
		{
			// escape squares = not defended by enemy pawns
			tss = NAttacks[pop_lsb(&fss)] & ~B->st().attacks[them][PAWN];
			// If escape square(s) are attacked and not defended by a pawn, then the knight is likely
			// to be trapped and we penalize it
			if (!(tss & ~(B->st().attacks[them][NO_PIECE] & ~B->st().attacks[us][PAWN])))
				e[us].op -= vOP;
			// in the endgame, we only look at king attacks, and incentivise the king to go and grab
			// the trapped knight
			if (!(tss & ~(B->st().attacks[them][KING] & ~B->st().attacks[us][PAWN])))
				e[us].eg -= vEP;
		}

		// Bishop trapped
		fss = B->get_pieces(us, BISHOP) & BishopTrap[us];
		while (fss)
		{
			const int fsq = pop_lsb(&fss);
			// See if the retreat path of the bishop is blocked by a defended pawn
			if (B->get_pieces(them, PAWN) & B->st().attacks[them][NO_PIECE] & PAttacks[them][fsq])
			{
				e[us].op -= vOP;
				// in the endgame, we only penalize if there's no escape via the 8th rank
				if (PAttacks[us][fsq] & B->st().attacks[them][KING])
					e[us].eg -= vEP;
			}
		}
	}
}

int EvalInfo::calc_phase() const
{
	static const int total = 4*(vN + vB + vR) + 2*vQ;
	return (B->st().piece_psq[WHITE] + B->st().piece_psq[BLACK]) * 1024 / total;
}

int EvalInfo::interpolate() const
{
	const int us = B->get_turn(), them = opp_color(us);
	const int phase = calc_phase();
	return (phase*(e[us].op-e[them].op) + (1024-phase)*(e[us].eg-e[them].eg)) / 1024;
}

class EvalCache
{
public:
	struct Entry
	{
		Key key48: 48;
		int16_t e;
	};

	EvalCache() { memset(data, 0, sizeof(data)); }
	Entry *probe(Key k) { return &data[k & (size-1)]; }

private:
	static const size_t size = 0x100000;
	Entry data[size];
};

EvalCache EC;

int eval(const Board& B)
{
	assert(!B.is_check());

	// en-passant square and castling rights do not affect the eval. so we can use the "unrefined"
	// key directly, and get (a little bit) more cache hits
	Key key = B.st().key;
	EvalCache::Entry *ce = EC.probe(key), tmp = {key};

	if (ce->key48 == tmp.key48)
		return ce->e;

	if (!B.st().last_move)
	{
		// The last move played is a null move. So we should have an entry for the same position,
		// with the turn of play revesed. And the eval is symetric, so we take advantage of it.
		Key key_rev = key ^ zob_turn;
		EvalCache::Entry *ce_rev = EC.probe(key_rev), tmp_rev = {key_rev};
		if (ce_rev->key48 == tmp_rev.key48)
			return -ce_rev->e;
	}

	EvalInfo ei(&B);
	ei.eval_material();
	ei.eval_mobility();
	ei.eval_pawns();
	ei.eval_safety();
	ei.eval_pieces();

	ce->key48 = key;
	return ce->e = ei.interpolate();
}
