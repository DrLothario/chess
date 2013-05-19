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
 *
 * Credits:
 * - PSQ generation inspired by Fruit 2.1 (Fabien Letouzey). Code is entirely genuine though, and
 * functionally different, albeit close.
*/
#include "psq.h"

const Eval Material[NB_PIECE] = {
	{vOP, vEP},
	{vN, vN},
	{vB, vB},
	{vR, vR},
	{vQ, vQ},
	{vK, vK}
};

Eval PsqTable[NB_PIECE][NB_SQUARE];

namespace
{
	/* Shape */
	const int Center[8]	= {-3, -1, +0, +1, +1, +0, -1, -3};
	const int NRank[8]	= {-2, -1, +0, +1, +2, +3, +2, +1};
	const int KFile[8]	= {+3, +4, +2, +0, +0, +2, +4, +3};
	const int KRank[8]	= {+1, +0, -2, -3, -4, -5, -6, -7};

	/* Weight */
	const int PFileOpening = 3;
	const int NCentreOpening = 5;
	const int NCentreEndgame = 5;
	const int NRankOpening = 5;
	const int BCentreOpening = 2;
	const int BCentreEndgame = 3;
	const int RFileOpening = 3;
	const int QCentreEndgame = 4;
	const int KCentreEndgame = 12;
	const int KFileOpening = 10;
	const int KRankOpening = 10;

	/* Adjustments */
	const int PCenterOpening = 20;
	const int BDiagonalOpening = 4;
	const int BBackRankOpening = 10;
	const int QBackRankOpening = 5;
	const int RSeventhRank = 8;

	Eval psq_bonus(int piece, int sq)
	{
		Eval e;
		e = {0,0};
		const int r = rank(sq), f = file(sq);

		switch (piece) {
		case PAWN:
			e.op += Center[f] * PFileOpening;
			if (sq == D5 || sq == E5 || sq == D3 || sq == E3)
				e.op += PCenterOpening / 2;
			else if (sq == D4 || sq == E4)
				e.op += PCenterOpening;
			break;
		case KNIGHT:
			e.op += (Center[r] + Center[f]) * NCentreOpening;
			e.eg += (Center[r] + Center[f]) * NCentreEndgame;
			e.op += NRank[r] * NRankOpening;
			break;
		case BISHOP:
			e.op += (Center[r] + Center[f]) * BCentreOpening;
			e.eg += (Center[r] + Center[f]) * BCentreEndgame;
			e.op -= BBackRankOpening * (r == RANK_1);
			e.op += BDiagonalOpening * (7 == r + f || r == f);
			break;
		case ROOK:
			e.op += Center[f] * RFileOpening;
			if (r == RANK_7) {
				e.op += RSeventhRank;
				e.eg += RSeventhRank;
			}
			break;
		case QUEEN:
			e.eg += (Center[r] + Center[f]) * QCentreEndgame;
			e.op -= QBackRankOpening * (r == RANK_1);
			break;
		case KING:
			e.eg += (Center[r] + Center[f]) * KCentreEndgame;
			e.op += KFile[f] * KFileOpening + KRank[r] * KRankOpening;
			break;
		}

		return e;
	}
}

void init_psq()
{
	for (int piece = PAWN; piece <= KING; ++piece) {
		for (int sq = A1; sq <= H8; ++sq) {
			Eval& e = PsqTable[piece][sq];
			e = psq_bonus(piece, sq);

			if (piece < KING)
				e += Material[piece];
		}

		/*for (int phase = OPENING; phase <= ENDGAME; ++phase) {
			for (int r = RANK_8; r >= RANK_1; --r)
				for (int f = FILE_A; f <= FILE_H; ++f) {
					int sq = square(r, f);
					Eval e = PsqTable[piece][sq];
					if (piece < KING)
						e -= Material[piece];
					std::cout << (phase == OPENING ? e.op : e.eg)
					          << (file(sq) == FILE_H ? '\n' : ',');
				}
			std::cout << std::endl;
		}*/
	}
}
