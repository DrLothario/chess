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
 * - Magic bitboard generation code from Unko, by Borko Boskovic.
*/
#include "bitboard.h"

namespace {

const int magic_bb_r_shift[NB_SQUARE] = {
	52, 53, 53, 53, 53, 53, 53, 52,
	53, 54, 54, 54, 54, 54, 54, 53,
	53, 54, 54, 54, 54, 54, 54, 53,
	53, 54, 54, 54, 54, 54, 54, 53,
	53, 54, 54, 54, 54, 54, 54, 53,
	53, 54, 54, 54, 54, 54, 54, 53,
	53, 54, 54, 54, 54, 54, 54, 53,
	53, 54, 54, 53, 53, 53, 53, 53
};

const Bitboard magic_bb_r_magics[NB_SQUARE] = {
	0x0080001020400080ull, 0x0040001000200040ull, 0x0080081000200080ull, 0x0080040800100080ull,
	0x0080020400080080ull, 0x0080010200040080ull, 0x0080008001000200ull, 0x0080002040800100ull,
	0x0000800020400080ull, 0x0000400020005000ull, 0x0000801000200080ull, 0x0000800800100080ull,
	0x0000800400080080ull, 0x0000800200040080ull, 0x0000800100020080ull, 0x0000800040800100ull,
	0x0000208000400080ull, 0x0000404000201000ull, 0x0000808010002000ull, 0x0000808008001000ull,
	0x0000808004000800ull, 0x0000808002000400ull, 0x0000010100020004ull, 0x0000020000408104ull,
	0x0000208080004000ull, 0x0000200040005000ull, 0x0000100080200080ull, 0x0000080080100080ull,
	0x0000040080080080ull, 0x0000020080040080ull, 0x0000010080800200ull, 0x0000800080004100ull,
	0x0000204000800080ull, 0x0000200040401000ull, 0x0000100080802000ull, 0x0000080080801000ull,
	0x0000040080800800ull, 0x0000020080800400ull, 0x0000020001010004ull, 0x0000800040800100ull,
	0x0000204000808000ull, 0x0000200040008080ull, 0x0000100020008080ull, 0x0000080010008080ull,
	0x0000040008008080ull, 0x0000020004008080ull, 0x0000010002008080ull, 0x0000004081020004ull,
	0x0000204000800080ull, 0x0000200040008080ull, 0x0000100020008080ull, 0x0000080010008080ull,
	0x0000040008008080ull, 0x0000020004008080ull, 0x0000800100020080ull, 0x0000800041000080ull,
	0x00FFFCDDFCED714Aull, 0x007FFCDDFCED714Aull, 0x003FFFCDFFD88096ull, 0x0000040810002101ull,
	0x0001000204080011ull, 0x0001000204000801ull, 0x0001000082000401ull, 0x0001FFFAABFAD1A2ull
};

const Bitboard magic_bb_r_mask[NB_SQUARE] = {
	0x000101010101017Eull, 0x000202020202027Cull, 0x000404040404047Aull, 0x0008080808080876ull,
	0x001010101010106Eull, 0x002020202020205Eull, 0x004040404040403Eull, 0x008080808080807Eull,
	0x0001010101017E00ull, 0x0002020202027C00ull, 0x0004040404047A00ull, 0x0008080808087600ull,
	0x0010101010106E00ull, 0x0020202020205E00ull, 0x0040404040403E00ull, 0x0080808080807E00ull,
	0x00010101017E0100ull, 0x00020202027C0200ull, 0x00040404047A0400ull, 0x0008080808760800ull,
	0x00101010106E1000ull, 0x00202020205E2000ull, 0x00404040403E4000ull, 0x00808080807E8000ull,
	0x000101017E010100ull, 0x000202027C020200ull, 0x000404047A040400ull, 0x0008080876080800ull,
	0x001010106E101000ull, 0x002020205E202000ull, 0x004040403E404000ull, 0x008080807E808000ull,
	0x0001017E01010100ull, 0x0002027C02020200ull, 0x0004047A04040400ull, 0x0008087608080800ull,
	0x0010106E10101000ull, 0x0020205E20202000ull, 0x0040403E40404000ull, 0x0080807E80808000ull,
	0x00017E0101010100ull, 0x00027C0202020200ull, 0x00047A0404040400ull, 0x0008760808080800ull,
	0x00106E1010101000ull, 0x00205E2020202000ull, 0x00403E4040404000ull, 0x00807E8080808000ull,
	0x007E010101010100ull, 0x007C020202020200ull, 0x007A040404040400ull, 0x0076080808080800ull,
	0x006E101010101000ull, 0x005E202020202000ull, 0x003E404040404000ull, 0x007E808080808000ull,
	0x7E01010101010100ull, 0x7C02020202020200ull, 0x7A04040404040400ull, 0x7608080808080800ull,
	0x6E10101010101000ull, 0x5E20202020202000ull, 0x3E40404040404000ull, 0x7E80808080808000ull
};

const int magic_bb_b_shift[NB_SQUARE] = {
	58, 59, 59, 59, 59, 59, 59, 58,
	59, 59, 59, 59, 59, 59, 59, 59,
	59, 59, 57, 57, 57, 57, 59, 59,
	59, 59, 57, 55, 55, 57, 59, 59,
	59, 59, 57, 55, 55, 57, 59, 59,
	59, 59, 57, 57, 57, 57, 59, 59,
	59, 59, 59, 59, 59, 59, 59, 59,
	58, 59, 59, 59, 59, 59, 59, 58
};

const Bitboard magic_bb_b_magics[NB_SQUARE] = {
	0x0002020202020200ull, 0x0002020202020000ull, 0x0004010202000000ull, 0x0004040080000000ull,
	0x0001104000000000ull, 0x0000821040000000ull, 0x0000410410400000ull, 0x0000104104104000ull,
	0x0000040404040400ull, 0x0000020202020200ull, 0x0000040102020000ull, 0x0000040400800000ull,
	0x0000011040000000ull, 0x0000008210400000ull, 0x0000004104104000ull, 0x0000002082082000ull,
	0x0004000808080800ull, 0x0002000404040400ull, 0x0001000202020200ull, 0x0000800802004000ull,
	0x0000800400A00000ull, 0x0000200100884000ull, 0x0000400082082000ull, 0x0000200041041000ull,
	0x0002080010101000ull, 0x0001040008080800ull, 0x0000208004010400ull, 0x0000404004010200ull,
	0x0000840000802000ull, 0x0000404002011000ull, 0x0000808001041000ull, 0x0000404000820800ull,
	0x0001041000202000ull, 0x0000820800101000ull, 0x0000104400080800ull, 0x0000020080080080ull,
	0x0000404040040100ull, 0x0000808100020100ull, 0x0001010100020800ull, 0x0000808080010400ull,
	0x0000820820004000ull, 0x0000410410002000ull, 0x0000082088001000ull, 0x0000002011000800ull,
	0x0000080100400400ull, 0x0001010101000200ull, 0x0002020202000400ull, 0x0001010101000200ull,
	0x0000410410400000ull, 0x0000208208200000ull, 0x0000002084100000ull, 0x0000000020880000ull,
	0x0000001002020000ull, 0x0000040408020000ull, 0x0004040404040000ull, 0x0002020202020000ull,
	0x0000104104104000ull, 0x0000002082082000ull, 0x0000000020841000ull, 0x0000000000208800ull,
	0x0000000010020200ull, 0x0000000404080200ull, 0x0000040404040400ull, 0x0002020202020200ull
};

const Bitboard magic_bb_b_mask[NB_SQUARE] = {
	0x0040201008040200ull, 0x0000402010080400ull, 0x0000004020100A00ull, 0x0000000040221400ull,
	0x0000000002442800ull, 0x0000000204085000ull, 0x0000020408102000ull, 0x0002040810204000ull,
	0x0020100804020000ull, 0x0040201008040000ull, 0x00004020100A0000ull, 0x0000004022140000ull,
	0x0000000244280000ull, 0x0000020408500000ull, 0x0002040810200000ull, 0x0004081020400000ull,
	0x0010080402000200ull, 0x0020100804000400ull, 0x004020100A000A00ull, 0x0000402214001400ull,
	0x0000024428002800ull, 0x0002040850005000ull, 0x0004081020002000ull, 0x0008102040004000ull,
	0x0008040200020400ull, 0x0010080400040800ull, 0x0020100A000A1000ull, 0x0040221400142200ull,
	0x0002442800284400ull, 0x0004085000500800ull, 0x0008102000201000ull, 0x0010204000402000ull,
	0x0004020002040800ull, 0x0008040004081000ull, 0x00100A000A102000ull, 0x0022140014224000ull,
	0x0044280028440200ull, 0x0008500050080400ull, 0x0010200020100800ull, 0x0020400040201000ull,
	0x0002000204081000ull, 0x0004000408102000ull, 0x000A000A10204000ull, 0x0014001422400000ull,
	0x0028002844020000ull, 0x0050005008040200ull, 0x0020002010080400ull, 0x0040004020100800ull,
	0x0000020408102000ull, 0x0000040810204000ull, 0x00000A1020400000ull, 0x0000142240000000ull,
	0x0000284402000000ull, 0x0000500804020000ull, 0x0000201008040200ull, 0x0000402010080400ull,
	0x0002040810204000ull, 0x0004081020400000ull, 0x000A102040000000ull, 0x0014224000000000ull,
	0x0028440200000000ull, 0x0050080402000000ull, 0x0020100804020000ull, 0x0040201008040200ull
};

Bitboard magic_bb_r_db[0x19000];
Bitboard magic_bb_b_db[0x1480];

const Bitboard* magic_bb_b_indices[NB_SQUARE] = {
	magic_bb_b_db+4992, magic_bb_b_db+2624, magic_bb_b_db+256,	magic_bb_b_db+896,
	magic_bb_b_db+1280, magic_bb_b_db+1664, magic_bb_b_db+4800, magic_bb_b_db+5120,
	magic_bb_b_db+2560, magic_bb_b_db+2656, magic_bb_b_db+288,  magic_bb_b_db+928,
	magic_bb_b_db+1312, magic_bb_b_db+1696, magic_bb_b_db+4832, magic_bb_b_db+4928,
	magic_bb_b_db+0,    magic_bb_b_db+128, magic_bb_b_db+320,	magic_bb_b_db+960,
	magic_bb_b_db+1344, magic_bb_b_db+1728, magic_bb_b_db+2304, magic_bb_b_db+2432,
	magic_bb_b_db+32,   magic_bb_b_db+160,  magic_bb_b_db+448,	magic_bb_b_db+2752,
	magic_bb_b_db+3776, magic_bb_b_db+1856, magic_bb_b_db+2336, magic_bb_b_db+2464,
	magic_bb_b_db+64,	magic_bb_b_db+192,  magic_bb_b_db+576,  magic_bb_b_db+3264,
	magic_bb_b_db+4288, magic_bb_b_db+1984, magic_bb_b_db+2368, magic_bb_b_db+2496,
	magic_bb_b_db+96,   magic_bb_b_db+224, magic_bb_b_db+704,	magic_bb_b_db+1088,
	magic_bb_b_db+1472, magic_bb_b_db+2112, magic_bb_b_db+2400, magic_bb_b_db+2528,
	magic_bb_b_db+2592, magic_bb_b_db+2688, magic_bb_b_db+832,	magic_bb_b_db+1216,
	magic_bb_b_db+1600, magic_bb_b_db+2240, magic_bb_b_db+4864, magic_bb_b_db+4960,
	magic_bb_b_db+5056, magic_bb_b_db+2720, magic_bb_b_db+864,  magic_bb_b_db+1248,
	magic_bb_b_db+1632, magic_bb_b_db+2272, magic_bb_b_db+4896, magic_bb_b_db+5184
};

const Bitboard* magic_bb_r_indices[64] = {
	magic_bb_r_db+86016, magic_bb_r_db+73728, magic_bb_r_db+36864, magic_bb_r_db+43008,
	magic_bb_r_db+47104, magic_bb_r_db+51200, magic_bb_r_db+77824, magic_bb_r_db+94208,
	magic_bb_r_db+69632, magic_bb_r_db+32768, magic_bb_r_db+38912, magic_bb_r_db+10240,
	magic_bb_r_db+14336, magic_bb_r_db+53248, magic_bb_r_db+57344, magic_bb_r_db+81920,
	magic_bb_r_db+24576, magic_bb_r_db+33792, magic_bb_r_db+6144,  magic_bb_r_db+11264,
	magic_bb_r_db+15360, magic_bb_r_db+18432, magic_bb_r_db+58368, magic_bb_r_db+61440,
	magic_bb_r_db+26624, magic_bb_r_db+4096,  magic_bb_r_db+7168,  magic_bb_r_db+0,
	magic_bb_r_db+2048,  magic_bb_r_db+19456, magic_bb_r_db+22528, magic_bb_r_db+63488,
	magic_bb_r_db+28672, magic_bb_r_db+5120,  magic_bb_r_db+8192,  magic_bb_r_db+1024,
	magic_bb_r_db+3072,  magic_bb_r_db+20480, magic_bb_r_db+23552, magic_bb_r_db+65536,
	magic_bb_r_db+30720, magic_bb_r_db+34816, magic_bb_r_db+9216,  magic_bb_r_db+12288,
	magic_bb_r_db+16384, magic_bb_r_db+21504, magic_bb_r_db+59392, magic_bb_r_db+67584,
	magic_bb_r_db+71680, magic_bb_r_db+35840, magic_bb_r_db+39936, magic_bb_r_db+13312,
	magic_bb_r_db+17408, magic_bb_r_db+54272, magic_bb_r_db+60416, magic_bb_r_db+83968,
	magic_bb_r_db+90112, magic_bb_r_db+75776, magic_bb_r_db+40960, magic_bb_r_db+45056,
	magic_bb_r_db+49152, magic_bb_r_db+55296, magic_bb_r_db+79872, magic_bb_r_db+98304
};

Bitboard init_magic_bb_occ(const int* sq, int numSq, Bitboard linocc)
{
	Bitboard ret = 0;
	for(int i = 0; i < numSq; i++)
		if (linocc & (1ULL << i))
			ret |= 1ULL << sq[i];
	return ret;
}

}	// namespace

void init_magics()
{
	static const int init_magic_bitpos64_db[64] = {
		63,  0, 58,  1, 59, 47, 53,  2,
		60, 39, 48, 27, 54, 33, 42,  3,
		61, 51, 37, 40, 49, 18, 28, 20,
		55, 30, 34, 11, 43, 14, 22,  4,
		62, 57, 46, 52, 38, 26, 32, 41,
		50, 36, 17, 19, 29, 10, 13, 21,
		56, 45, 25, 31, 35, 16,  9, 12,
		44, 24, 15,  8, 23,  7,  6,  5
	};

	static Bitboard* const magic_bb_b_indices2[64] = {
		magic_bb_b_db+4992, magic_bb_b_db+2624, magic_bb_b_db+256,
		magic_bb_b_db+896,  magic_bb_b_db+1280, magic_bb_b_db+1664,
		magic_bb_b_db+4800, magic_bb_b_db+5120, magic_bb_b_db+2560,
		magic_bb_b_db+2656, magic_bb_b_db+288,  magic_bb_b_db+928,
		magic_bb_b_db+1312, magic_bb_b_db+1696, magic_bb_b_db+4832,
		magic_bb_b_db+4928, magic_bb_b_db+0,    magic_bb_b_db+128,
		magic_bb_b_db+320,  magic_bb_b_db+960,  magic_bb_b_db+1344,
		magic_bb_b_db+1728, magic_bb_b_db+2304, magic_bb_b_db+2432,
		magic_bb_b_db+32,   magic_bb_b_db+160,  magic_bb_b_db+448,
		magic_bb_b_db+2752, magic_bb_b_db+3776, magic_bb_b_db+1856,
		magic_bb_b_db+2336, magic_bb_b_db+2464, magic_bb_b_db+64,
		magic_bb_b_db+192,  magic_bb_b_db+576,  magic_bb_b_db+3264,
		magic_bb_b_db+4288, magic_bb_b_db+1984, magic_bb_b_db+2368,
		magic_bb_b_db+2496, magic_bb_b_db+96,   magic_bb_b_db+224,
		magic_bb_b_db+704,  magic_bb_b_db+1088, magic_bb_b_db+1472,
		magic_bb_b_db+2112, magic_bb_b_db+2400, magic_bb_b_db+2528,
		magic_bb_b_db+2592, magic_bb_b_db+2688, magic_bb_b_db+832,
		magic_bb_b_db+1216, magic_bb_b_db+1600, magic_bb_b_db+2240,
		magic_bb_b_db+4864, magic_bb_b_db+4960, magic_bb_b_db+5056,
		magic_bb_b_db+2720, magic_bb_b_db+864,  magic_bb_b_db+1248,
		magic_bb_b_db+1632, magic_bb_b_db+2272, magic_bb_b_db+4896,
		magic_bb_b_db+5184
	};

	static Bitboard* const magic_bb_r_indices2[64] = {
		magic_bb_r_db+86016, magic_bb_r_db+73728, magic_bb_r_db+36864,
		magic_bb_r_db+43008, magic_bb_r_db+47104, magic_bb_r_db+51200,
		magic_bb_r_db+77824, magic_bb_r_db+94208, magic_bb_r_db+69632,
		magic_bb_r_db+32768, magic_bb_r_db+38912, magic_bb_r_db+10240,
		magic_bb_r_db+14336, magic_bb_r_db+53248, magic_bb_r_db+57344,
		magic_bb_r_db+81920, magic_bb_r_db+24576, magic_bb_r_db+33792,
		magic_bb_r_db+6144,  magic_bb_r_db+11264, magic_bb_r_db+15360,
		magic_bb_r_db+18432, magic_bb_r_db+58368, magic_bb_r_db+61440,
		magic_bb_r_db+26624, magic_bb_r_db+4096,  magic_bb_r_db+7168,
		magic_bb_r_db+0,     magic_bb_r_db+2048,  magic_bb_r_db+19456,
		magic_bb_r_db+22528, magic_bb_r_db+63488, magic_bb_r_db+28672,
		magic_bb_r_db+5120,  magic_bb_r_db+8192,  magic_bb_r_db+1024,
		magic_bb_r_db+3072,  magic_bb_r_db+20480, magic_bb_r_db+23552,
		magic_bb_r_db+65536, magic_bb_r_db+30720, magic_bb_r_db+34816,
		magic_bb_r_db+9216,  magic_bb_r_db+12288, magic_bb_r_db+16384,
		magic_bb_r_db+21504, magic_bb_r_db+59392, magic_bb_r_db+67584,
		magic_bb_r_db+71680, magic_bb_r_db+35840, magic_bb_r_db+39936,
		magic_bb_r_db+13312, magic_bb_r_db+17408, magic_bb_r_db+54272,
		magic_bb_r_db+60416, magic_bb_r_db+83968, magic_bb_r_db+90112,
		magic_bb_r_db+75776, magic_bb_r_db+40960, magic_bb_r_db+45056,
		magic_bb_r_db+49152, magic_bb_r_db+55296, magic_bb_r_db+79872,
		magic_bb_r_db+98304
	};

	static const int Bdir[4][2] = { {-1,-1}, {-1, 1}, { 1,-1}, { 1, 1} };
	static const int Rdir[4][2] = { {-1, 0}, { 0,-1}, { 0, 1}, { 1, 0} };

	for(int i = A1; i <= H8; i++) {
		int sq[NB_SQUARE];
		int numSq = 0;
		Bitboard temp = magic_bb_b_mask[i];
		while(temp) {
			Bitboard bit = temp & -temp;
			sq[numSq++] = init_magic_bitpos64_db[(bit * 0x07EDD5E59A4E28C2ull) >> 58];
			temp ^= bit;
		}
		for(temp = 0; temp < (1ULL << numSq); temp++) {
			Bitboard tempocc = init_magic_bb_occ(sq, numSq, temp);
			*(magic_bb_b_indices2[i] + ((tempocc * magic_bb_b_magics[i]) >> magic_bb_b_shift[i])) =
				calc_sliding_attacks(i, tempocc, Bdir);
		}
	}

	for(int i = A1; i <= H8; i++) {
		int sq[NB_SQUARE];
		int numSq = 0;
		Bitboard temp = magic_bb_r_mask[i];
		while(temp) {
			Bitboard bit = temp & -temp;
			sq[numSq++] = init_magic_bitpos64_db[(bit * 0x07EDD5E59A4E28C2ull) >> 58];
			temp ^= bit;
		}
		for(temp = 0; temp < (1ULL << numSq); temp++) {
			Bitboard tempocc = init_magic_bb_occ(sq, numSq, temp);
			*(magic_bb_r_indices2[i] + ((tempocc * magic_bb_r_magics[i]) >> magic_bb_r_shift[i])) =
				calc_sliding_attacks(i, tempocc, Rdir);
		}
	}
}

Bitboard bishop_attack(int sq, Bitboard occ)
{
	assert(square_ok(sq));
	return *(magic_bb_b_indices[sq]
			 + (((occ & magic_bb_b_mask[sq]) * magic_bb_b_magics[sq]) >> magic_bb_b_shift[sq]));
}

Bitboard rook_attack(int sq, Bitboard occ)
{
	assert(square_ok(sq));
	return *(magic_bb_r_indices[sq]
			 + (((occ & magic_bb_r_mask[sq]) * magic_bb_r_magics[sq]) >> magic_bb_r_shift[sq]));
}
