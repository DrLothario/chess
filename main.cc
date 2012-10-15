/*
 * Zinc, an UCI chess interface. Copyright (C) 2012 Lucas Braesch.
 * 
 * Zinc is free software: you can redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Zinc is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along with this program. If not,
 * see <http://www.gnu.org/licenses/>.
*/
#include <map>
#include "types.h"
#include "match.h"
#include "epd.h"

int main(int argc, char **argv)
{
	init_bitboard();

	Engine E[NB_COLOR];
	E[White].create(argv[1]);
	E[Black].create(argv[2]);

	std::map<Result, std::string> result_desc;
	result_desc[ResultMate] = "check mate";
	result_desc[ResultThreefold] = "3-fold repetition";
	result_desc[Result50Move] = "50-move rule";
	result_desc[ResultMaterial] = "insufficient material";
	result_desc[ResultStalemate] = "stalemate";
	result_desc[ResultIllegalMove] = "illegal move";
	result_desc[ResultNone] = "ERROR";

	std::map<Color, std::string> winner_desc;
	winner_desc[White] = "White wins";
	winner_desc[Black] = "Black wins";
	winner_desc[NoColor] = "Draw";
	
	MatchResult match_result;
	Engine::SearchParam sp;
	sp.movetime = 100;
	
	match_result = match(E, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", sp);
	
	std::cout << winner_desc[match_result.winner] << " by "
		<< result_desc[match_result.result] << std::endl;
}
/*
#include <iostream>
#include "epd.h"

int main()
{
	EPD epd("/home/lucas/Chess/test.epd");
	for (int i = 0; i < 10; i++)
		std::cout << epd.next() << std::endl;
}*/