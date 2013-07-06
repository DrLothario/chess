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
 * Credits: Bob Jenkins' 64-bit PRNG http://www.burtleburtle.net/bob/rand/smallprng.html
 * - passes the DieHarder test suite, with various seeds, even zero (I've tested it).
 * - much simpler and faster than complex generators (eg. Mersenne Twister).
 * - not cryptographically secure, but I don't care (for Zobrist hashing).
*/
#pragma once
#include <cinttypes>

struct PRNG {
	PRNG() {
		init();
	}

	std::uint64_t rand() {
		const std::uint64_t e = a - rotate(b,  7);
		a = b ^ rotate(c, 13);
		b = c + rotate(d, 37);
		c = d + e;
		return d = e + a;
	}

	void init(std::uint64_t seed = 0) {
		a = 0xf1ea5eed, b = c = d = seed;
		for (int i = 0; i < 20; ++i)
			rand();
	}

private:
	// generator state
	std::uint64_t a, b, c, d;

	std::uint64_t rotate(std::uint64_t x, std::uint64_t k) const {
		return (x << k) | (x >> (64 - k));
	}
};
