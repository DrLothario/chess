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
#include <string>
#include "bitboard.h"

/* Runs a set of perft(). This is the unit test to validate ANY modification in the board code. */
bool test_perft();

#define MAX_PLY		0x400	// max number of plies for a game
#define MAX_MOVES	0x80	// max number of legal moves

/* Castling flags: those are for White, use << 2 for Black */
enum {
    OO = 1,		// King side castle (OO = chess notation)
    OOO = 2		// Queen side castle (OOO = chess notation)
};

enum {
	NORMAL,
	EN_PASSANT,
	PROMOTION,
	CASTLING
};

struct move_t {
	uint16_t fsq:6, tsq:6;
	uint16_t prom:2;	// 0=Knight...3=Queen
	uint16_t flag:2;	// 0=normal,1=ep,2=prom,3=castling

	bool operator== (move_t m) const {
		return fsq == m.fsq && tsq == m.tsq && flag == m.flag;
	}
	
	int get_prom() const {
		assert(flag == PROMOTION);
		return prom + KNIGHT;
	}
	
	void set_prom(int piece) {
		assert(KNIGHT <= piece && piece <= QUEEN);
		prom = piece - KNIGHT;
	}
};

struct game_info {
	int capture;				// piece just captured
	int epsq;					// en passant square
	int crights;				// castling rights, 4 bits in FEN order KQkq
	move_t last_move;			// last move played (for undo)
	Key key;					// base zobrist key
	Bitboard pinned, dcheckers;	// pinned and discovery checkers for turn
	Bitboard attacked;			// squares attacked by opp_color(turn)
	Bitboard checkers;			// pieces checking turn's King
	Bitboard occ;				// occupancy
	int rule50;					// counter for the 50 move rule

	Bitboard epsq_bb() const { return epsq ? (1ULL << epsq) : 0; }
};

class Board
{
	Bitboard b[NB_COLOR][NB_PIECE];
	Bitboard all[NB_COLOR];
	int piece_on[NB_SQUARE];
	game_info game_stack[MAX_PLY], *_st;
	int turn;
	int king_pos[NB_COLOR];
	int move_count;				// full move count, as per FEN standard
	bool initialized;

	void clear();
	void set_square(int color, int piece, int sq, bool play = true);
	void clear_square(int color, int piece, int sq, bool play = true);

	Key calc_key() const;
	Bitboard calc_attacks(int color) const;
	Bitboard calc_checkers(int kcolor) const;
	Bitboard hidden_checkers(bool find_pins, int color) const;

public:
	const game_info& st() const;

	int get_turn() const;
	int get_move_count() const;
	int get_king_pos(int c) const;
	int get_color_on(int sq) const;
	int get_piece_on(int sq) const;

	Bitboard get_pieces(int color) const;
	Bitboard get_pieces(int color, int piece) const;
	Bitboard get_N() const;
	Bitboard get_K() const;
	Bitboard get_RQ() const;
	Bitboard get_BQ() const;
	Bitboard get_RQ(int color) const;	
	Bitboard get_BQ(int color) const;

	void set_fen(const std::string& fen);
	std::string get_fen() const;

	void play(move_t m);
	void undo();
};

extern const std::string PieceLabel[NB_COLOR];
extern std::ostream& operator<< (std::ostream& ostrm, const Board& B);

inline int pawn_push(int color, int sq)
{
	assert(color_ok(color) && rank(sq) >= RANK_2 && rank(sq) <= RANK_7);
	return color ? sq - 8 : sq + 8;
}
