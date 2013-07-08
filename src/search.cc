/*
 * DiscoCheck, an UCI chess engine. Copyright (C) 2011-2013 Lucas Braesch.
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
#include <chrono>
#include "search.h"
#include "uci.h"
#include "eval.h"
#include "movesort.h"
#include "prng.h"

TTable TT;

std::uint64_t node_count;
std::uint64_t PollingFrequency;

using namespace std::chrono;

namespace {

const int MAX_PLY = 0x80;
const int MATE = 32000;
const int QS_LIMIT = -8;

bool can_abort;
struct AbortSearch {};
struct ForcedMove {};

std::uint64_t node_limit;
int time_limit[2], time_allowed;
time_point<high_resolution_clock> start;

void node_poll(Board& B);
void time_alloc(const SearchLimits& sl, int result[2]);

History H;
Refutation R;

int score_to_tt(int score, int ply);
int score_from_tt(int tt_score, int ply);

bool can_return_tt(bool is_pv, const TTable::Entry *tte, int depth, int beta, int ply);

bool is_mate_score(int score)
{
	return std::abs(score) >= MATE - MAX_PLY;
}

int mated_in(int ply)
{
	return ply - MATE;
}

int mate_in(int ply)
{
	return MATE - ply;
}

int null_reduction(int depth)
{
	return 3 + depth / 4;
}

const int RazorMargin[4] = {0, 2 * vEP, 2 * vEP + vEP / 2, 3 * vEP};
const int EvalMargin[4]	 = {0, vEP, vN, vQ};

int DrawScore[NB_COLOR];	// Contempt draw score by color

move_t best;
int search(Board& B, int alpha, int beta, int depth, int node_type, SearchInfo *ss);
int qsearch(Board& B, int alpha, int beta, int depth, int node_type, SearchInfo *ss);

}	// namespace

move_t bestmove(Board& B, const SearchLimits& sl)
{
	start = high_resolution_clock::now();

	SearchInfo ss[MAX_PLY + 1 - QS_LIMIT];
	for (int ply = 0; ply < MAX_PLY + 1 - QS_LIMIT; ++ply)
		ss[ply].clear(ply);

	node_count = 0;
	node_limit = sl.nodes;
	time_alloc(sl, time_limit);
	best = move_t(0);

	// We can only abort the search once iteration 1 is finished. In extreme situations (eg. fixed
	// nodes), the SearchLimits sl could trigger a search abortion before that, which is disastrous,
	// as the best move could be illegal or completely stupid.
	can_abort = false;

	H.clear();
	R.clear();
	TT.new_search();
	B.set_unwind();		// remember the Board state

	// Calculate the value of a draw by chess rules, for both colors (contempt option)
	const int us = B.get_turn(), them = opp_color(us);
	DrawScore[us] = -uci::Contempt;
	DrawScore[them] = +uci::Contempt;

	const int max_depth = sl.depth ? std::min(MAX_PLY - 1, sl.depth) : MAX_PLY - 1;

	for (int depth = 1, alpha = -INF, beta = +INF; depth <= max_depth; depth++) {
		// iterative deepening loop

		int score, delta = 16;
		// set time allowance to normal, and divide by two if we're in an "easy" recapture situation
		time_allowed = time_limit[0] >> (best && calc_see(B, best) > 0);

		for (;;) {
			// Aspiration loop

			try {
				score = search(B, alpha, beta, depth, PV, ss);
			} catch (AbortSearch e) {
				return best;
			} catch (ForcedMove e) {
				return best = ss->best;
			}

			std::cout << "info score cp " << score << " depth " << depth << " nodes " << node_count
					  << " time " << duration_cast<milliseconds>(high_resolution_clock::now() - start).count();

			if (alpha < score && score < beta) {
				// score is within bounds
				if (depth >= 4 && !is_mate_score(score)) {
					// set aspiration window for the next depth (so aspiration starts at depth 5)
					alpha = score - delta;
					beta = score + delta;
				}
				// stop the aspiration loop
				break;
			} else {
				// score is outside bounds: resize window and double delta
				if (score <= alpha) {
					alpha -= delta;
					std::cout << " upperbound" << std::endl;
				} else if (score >= beta) {
					beta += delta;
					std::cout << " lowerbound" << std::endl;
				}
				delta *= 2;

				// increase time_allowed, to try to finish the current depth iteration
				time_allowed = time_limit[1];
			}
		}

		// Here we know for sure that iteration 1 is finished. Aborting before the end of
		// iteration 1 is disastrous, and can return a null or stupid move
		can_abort = true;

		// Display best move for this iteration
		std::cout << " pv " << move_to_string(best) << std::endl;
	}

	return best;
}

namespace {

int search(Board& B, int alpha, int beta, int depth, int node_type, SearchInfo *ss)
{
	assert(alpha < beta && (node_type == PV || alpha + 1 == beta));

	if (depth <= 0 || ss->ply >= MAX_PLY)
		return qsearch(B, alpha, beta, depth, node_type, ss);

	const Key key = B.get_key();
	TT.prefetch(key);

	node_poll(B);

	const bool root = !ss->ply, in_check = B.is_check();
	const int old_alpha = alpha, static_node_type = node_type;
	int best_score = -INF;
	ss->best = move_t(0);

	if (B.is_draw())
		return DrawScore[B.get_turn()];

	// mate distance pruning
	alpha = std::max(alpha, mated_in(ss->ply));
	beta = std::min(beta, mate_in(ss->ply + 1));
	if (alpha >= beta) {
		assert(!root);
		return alpha;
	}

	const Bitboard hanging = hanging_pieces(B, B.get_turn());

	// TT lookup
	const TTable::Entry *tte = TT.probe(key);
	if (tte) {
		if (!root && can_return_tt(node_type == PV, tte, depth, beta, ss->ply)) {
			TT.refresh(tte);
			return score_from_tt(tte->score, ss->ply);
		}
		ss->eval = tte->eval;
		ss->best = tte->move;
	} else
		ss->eval = in_check ? -INF : (ss->null_child ? -(ss - 1)->eval : eval::symmetric_eval(B));

	// Stand pat score (adjusted for tempo and hanging pieces)
	const int stand_pat = ss->eval + eval::asymmetric_eval(B);

	// Eval pruning
	if ( depth <= 3 && node_type != PV
		 && !in_check && !is_mate_score(beta)
		 && stand_pat >= beta + EvalMargin[depth]
		 && B.st().piece_psq[B.get_turn()] )
		return stand_pat;

	// Razoring
	if ( depth <= 3 && node_type != PV
		 && !in_check && !is_mate_score(beta) ) {
		const int threshold = beta - RazorMargin[depth];
		if (ss->eval < threshold) {
			const int score = qsearch(B, threshold - 1, threshold, 0, All, ss + 1);
			if (score < threshold)
				return score;
		}
	}

	// Null move pruning
	move_t threat_move = move_t(0);
	if ( ss->eval >= beta	// eval symmetry prevents double null moves
		 && !ss->skip_null && node_type != PV
		 && !in_check && !is_mate_score(beta)
		 && B.st().piece_psq[B.get_turn()] ) {
		const int reduction = null_reduction(depth) + (ss->eval - vOP >= beta);

		B.play(move_t(0));
		(ss + 1)->null_child = true;
		int score = -search(B, -beta, -alpha, depth - reduction, All, ss + 1);
		(ss + 1)->null_child = false;
		B.undo();

		if (score >= beta)		// null search fails high
			return score < mate_in(MAX_PLY)
				   ? score		// fail soft
				   : beta;		// *but* do not return an unproven mate
		else {
			threat_move = (ss + 1)->best;
			if (score <= mated_in(MAX_PLY) && (ss - 1)->reduction)
				++depth;
		}
	}

	// Internal Iterative Deepening
	if ( (!tte || !tte->move || tte->depth <= 0)
		 && depth >= (node_type == PV ? 4 : 7) ) {
		ss->skip_null = true;
		search(B, alpha, beta, node_type == PV ? depth - 2 : depth / 2, node_type, ss);
		ss->skip_null = false;
	}

	MoveSort MS(&B, depth, ss, &H, &R);
	const move_t refutation = R.get_refutation(B.get_dm_key());

	int cnt = 0, LMR = 0, see;
	while ( alpha < beta && (ss->m = MS.next(&see)) ) {
		++cnt;
		const int check = move_is_check(B, ss->m);

		// check extension
		int new_depth;
		if (check && (check == DISCO_CHECK || see >= 0) )
			// extend relevant checks
			new_depth = depth;
		else if (MS.get_count() == 1)
			// extend forced replies
			new_depth = depth;
		else
			new_depth = depth - 1;

		// move properties
		const bool first = cnt == 1;
		const bool capture = move_is_cop(B, ss->m);
		const int hscore = capture ? 0 : H.get(B, ss->m);
		const bool bad_quiet = !capture && (hscore < 0 || (hscore == 0 && see < 0));
		const bool bad_capture = capture && see < 0;
		// dangerous movea are not reduced
		const bool dangerous = check
							   || ss->m == ss->killer[0]
							   || ss->m == ss->killer[1]
							   || ss->m == refutation
							   || (move_is_pawn_threat(B, ss->m) && see >= 0)
							   || (ss->m.flag() == CASTLING);

		if (!capture && !dangerous && !in_check && !root) {
			// Move count pruning
			if ( depth <= 6 && node_type != PV
				 && LMR >= 3 + depth * depth
				 && alpha > mated_in(MAX_PLY)
				 && (see < 0 || !refute(B, ss->m, threat_move)) ) {
				best_score = std::max(best_score, std::min(alpha, stand_pat + see));
				continue;
			}

			// SEE pruning near the leaves
			if (new_depth <= 1 && see < 0) {
				best_score = std::max(best_score, std::min(alpha, stand_pat + see));
				continue;
			}
		}

		// reduction decision
		ss->reduction = !first && (bad_capture || bad_quiet) && !dangerous;
		if (ss->reduction && !capture)
			ss->reduction += ++LMR >= (static_node_type == Cut ? 2 : 3) + 8 / depth;

		// do not LMR into the QS
		if (new_depth - ss->reduction <= 0)
			ss->reduction = 0;

		B.play(ss->m);

		// PVS
		int score;
		if (first)
			// search full window full depth
			// Note that the full window is a zero window at non PV nodes
			// "-node_type" effectively does PV->PV Cut<->All
			score = -search(B, -beta, -alpha, new_depth, -node_type, ss + 1);
		else {
			// Cut node: If the first move didn't produce the expected cutoff, then we are
			// unlikely to get a cutoff at this node, which becomes an All node, so that its
			// children are Cut nodes
			if (node_type == Cut)
				node_type = All;

			// zero window search (reduced)
			score = -search(B, -alpha - 1, -alpha, new_depth - ss->reduction,
							node_type == PV ? Cut : -node_type, ss + 1);

			// doesn't fail low: verify at full depth, with zero window
			if (score > alpha && ss->reduction)
				score = -search(B, -alpha - 1, -alpha, new_depth, All, ss + 1);

			// still doesn't fail low at PV node: full depth and full window
			if (node_type == PV && score > alpha)
				score = -search(B, -beta, -alpha, new_depth , PV, ss + 1);
		}

		B.undo();

		if (score > best_score) {
			best_score = score;
			alpha = std::max(alpha, score);
			ss->best = ss->m;
			if (root)
				best = ss->m;
		}
	}

	if (!MS.get_count()) {
		// mated or stalemated
		assert(!root);
		return in_check ? mated_in(ss->ply) : DrawScore[B.get_turn()];
	} else if (root && MS.get_count() == 1)
		// forced move at the root node, play instantly and prevent further iterative deepening
		throw ForcedMove();

	// update TT
	node_type = best_score <= old_alpha ? All : best_score >= beta ? Cut : PV;
	TT.store(key, node_type, depth, score_to_tt(best_score, ss->ply), ss->eval, ss->best);

	// best move is quiet: update killers and history
	if (ss->best && !move_is_cop(B, ss->best)) {
		// update killers on a LIFO basis
		if (ss->killer[0] != ss->best) {
			ss->killer[1] = ss->killer[0];
			ss->killer[0] = ss->best;
		}

		// update history table
		// mark ss->best as good, and all other moves searched as bad
		move_t m;
		int bonus = std::min(depth * depth, (int)History::Max);
		if (hanging) bonus /= 2;
		while ( (m = MS.previous()) )
			if (!move_is_cop(B, m))
				H.add(B, m, m == ss->best ? bonus : -bonus);

		// update double move refutation hash table
		R.set_refutation(B.get_dm_key(), ss->best);
	}

	return best_score;
}

int qsearch(Board& B, int alpha, int beta, int depth, int node_type, SearchInfo *ss)
{
	assert(depth <= 0);
	assert(alpha < beta && (node_type == PV || alpha + 1 == beta));

	const Key key = B.get_key();
	TT.prefetch(key);
	node_poll(B);

	const bool in_check = B.is_check();
	int best_score = -INF, old_alpha = alpha;
	ss->best = move_t(0);

	if (B.is_draw())
		return DrawScore[B.get_turn()];

	// TT lookup
	const TTable::Entry *tte = TT.probe(key);
	if (tte) {
		if (can_return_tt(node_type == PV, tte, depth, beta, ss->ply)) {
			TT.refresh(tte);
			return score_from_tt(tte->score, ss->ply);
		}
		ss->eval = tte->eval;
		ss->best = tte->move;
	} else
		ss->eval = in_check ? -INF : (ss->null_child ? -(ss - 1)->eval : eval::symmetric_eval(B));

	// stand pat
	if (!in_check) {
		best_score = ss->eval + eval::asymmetric_eval(B);
		alpha = std::max(alpha, best_score);
		if (alpha >= beta)
			return alpha;
	}

	MoveSort MS(&B, depth, ss, &H, nullptr);
	int see;
	const int fut_base = ss->eval + vEP / 2;

	while ( alpha < beta && (ss->m = MS.next(&see)) ) {
		int check = move_is_check(B, ss->m);

		// Futility pruning
		if (!check && !in_check && node_type != PV) {
			// opt_score = current eval + some margin + max material gain of the move
			const int opt_score = fut_base
								  + Material[B.get_piece_on(ss->m.tsq())].eg
								  + (ss->m.flag() == EN_PASSANT ? vEP : 0)
								  + (ss->m.flag() == PROMOTION ? Material[ss->m.prom()].eg - vOP : 0);

			// still can't raise alpha, skip
			if (opt_score <= alpha) {
				best_score = std::max(best_score, opt_score);	// beware of fail soft side effect
				continue;
			}

			// the "SEE proxy" tells us we are unlikely to raise alpha, skip if depth < 0
			if (fut_base <= alpha && depth < 0 && see <= 0) {
				best_score = std::max(best_score, fut_base);	// beware of fail soft side effect
				continue;
			}
		}

		// SEE pruning
		if (!in_check && check != DISCO_CHECK && see < 0)
			continue;

		// recursion
		int score;
		if (depth <= QS_LIMIT && !in_check)		// prevent qsearch explosion
			score = ss->eval + see;
		else {
			B.play(ss->m);
			score = -qsearch(B, -beta, -alpha, depth - 1, -node_type, ss + 1);
			B.undo();
		}

		if (score > best_score) {
			best_score = score;
			alpha = std::max(alpha, score);
			ss->best = ss->m;
		}
	}

	if (B.is_check() && !MS.get_count())
		return mated_in(ss->ply);

	// update TT
	node_type = best_score <= old_alpha ? All : best_score >= beta ? Cut : PV;
	TT.store(key, node_type, depth, score_to_tt(best_score, ss->ply), ss->eval, ss->best);

	return best_score;
}

void node_poll(Board &B)
{
	if (!(++node_count & (PollingFrequency - 1)) && can_abort) {
		bool abort = false;

		// abort search because node limit exceeded
		if (node_limit && node_count >= node_limit)
			abort = true;
		// abort search because time limit exceeded
		else if (time_allowed && duration_cast<milliseconds>
				 (high_resolution_clock::now() - start).count() > time_allowed)
			abort = true;
		// abort when UCI "stop" command is received (involves some non standard I/O)
		else if (uci::stop())
			abort = true;

		if (abort)
			throw AbortSearch();
	}
}

int score_to_tt(int score, int ply)
/* mate scores from the search, must be adjusted to be written in the TT. For example, if we find a
 * mate in 10 plies from the current position, it will be scored mate_in(15) by the search and must
 * be entered mate_in(10) in the TT */
{
	return score >= mate_in(MAX_PLY) ? score + ply :
		   score <= mated_in(MAX_PLY) ? score - ply : score;
}

int score_from_tt(int tt_score, int ply)
/* mate scores from the TT need to be adjusted. For example, if we find a mate in 10 in the TT at
 * ply 5, then we effectively have a mate in 15 plies (from the root) */
{
	return tt_score >= mate_in(MAX_PLY) ? tt_score - ply :
		   tt_score <= mated_in(MAX_PLY) ? tt_score + ply : tt_score;
}

bool can_return_tt(bool is_pv, const TTable::Entry *tte, int depth, int beta, int ply)
/* PV nodes: return only exact scores
 * non PV nodes: return fail high/low scores. Mate scores are also trusted, regardless of the
 * depth. This idea is from StockFish, and although it's not totally sound, it seems to work. */
{
	const bool depth_ok = tte->depth >= depth;

	if (is_pv)
		return depth_ok && tte->node_type() == PV;
	else {
		const int tt_score = score_from_tt(tte->score, ply);
		return (depth_ok
				|| tt_score >= std::max(mate_in(MAX_PLY), beta)
				|| tt_score < std::min(mated_in(MAX_PLY), beta))
			   && ((tte->node_type() == Cut && tt_score >= beta)
				   || (tte->node_type() == All && tt_score < beta));
	}
}

void time_alloc(const SearchLimits& sl, int result[2])
{
	if (sl.movetime > 0)
		result[0] = result[1] = sl.movetime;
	else if (sl.time > 0 || sl.inc > 0) {
		static const int time_buffer = 100;
		int movestogo = sl.movestogo > 0 ? sl.movestogo : 30;
		result[0] = std::max(std::min(sl.time / movestogo + sl.inc, sl.time - time_buffer), 1);
		result[1] = std::max(std::min(sl.time / (1 + movestogo / 2) + sl.inc, sl.time - time_buffer), 1);
	}
}

}	// namespace

