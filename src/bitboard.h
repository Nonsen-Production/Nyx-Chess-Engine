#pragma once

#include <cstdint>

namespace Bitboards {

// Pre-computed attack tables
extern std::uint64_t KNIGHT_ATTACKS[64];
extern std::uint64_t KING_ATTACKS[64];
extern std::uint64_t PAWN_ATTACKS[2][64]; // [color][square], 0=white, 1=black

// File and rank masks
extern std::uint64_t FILE_BB[8];
extern std::uint64_t RANK_BB[8];

// === Magic Bitboards for sliding pieces ===

struct MagicEntry {
  std::uint64_t mask;     // Relevant occupancy mask (excludes edges)
  std::uint64_t magic;    // Magic number for hashing
  int shift;              // 64 - number of relevant bits
  std::uint64_t* attacks; // Pointer into attack table
};

extern MagicEntry BISHOP_MAGICS[64];
extern MagicEntry ROOK_MAGICS[64];

// Attack table storage
extern std::uint64_t BISHOP_TABLE[5248];  // Total bishop attack entries
extern std::uint64_t ROOK_TABLE[102400];  // Total rook attack entries

// Initialize all attack tables. Must be called once at startup.
void init();

// O(1) magic bitboard sliding piece attack lookups
inline std::uint64_t bishopAttacks(int sq, std::uint64_t occ) {
  const MagicEntry& m = BISHOP_MAGICS[sq];
  return m.attacks[((occ & m.mask) * m.magic) >> m.shift];
}

inline std::uint64_t rookAttacks(int sq, std::uint64_t occ) {
  const MagicEntry& m = ROOK_MAGICS[sq];
  return m.attacks[((occ & m.mask) * m.magic) >> m.shift];
}

inline std::uint64_t queenAttacks(int sq, std::uint64_t occ) {
  return bishopAttacks(sq, occ) | rookAttacks(sq, occ);
}

// Utility
inline int popcount(std::uint64_t bb) {
  return __builtin_popcountll(static_cast<unsigned long long>(bb));
}

inline int lsb(std::uint64_t bb) {
  return __builtin_ctzll(static_cast<unsigned long long>(bb));
}

inline int popLSB(std::uint64_t &bb) {
  int sq = lsb(bb);
  bb &= bb - 1;
  return sq;
}

} // namespace Bitboards
