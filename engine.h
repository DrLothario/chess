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
#pragma once
#include <set>
#include <string>
#include "process.h"
#include "chessclock.h"

class Engine
{
public:
	struct Err {};
	struct SyntaxErr: Err {};

	struct Option
	{
		struct Err {};
		struct NotFound: Err {};
		struct OutOfBounds: Err {};

		enum Type { Boolean, Integer };
		Type type;
		std::string name;
		int value, min, max;

		bool operator< (const Option& o) const;
	};
	
	std::string engine_name;
	ChessClock clk;
	
	void create(const char *cmd) throw (Process::Err, Err);
	void set_option(const std::string& name, Option::Type type, int value) throw (Option::Err);
	void set_position(const std::string& fen, const std::string& moves) const throw (Process::Err);
	std::string search(Color color) const throw (Process::Err);

private:
	Process p;
	std::set<Option> options;

	void sync() const;
};

inline bool Engine::Option::operator< (const Option& o) const
{
	return type < o.type && name < o.name;
}