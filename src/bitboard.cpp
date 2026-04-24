#include "bitboard.h"

namespace Bitboards {

std::uint64_t KNIGHT_ATTACKS[64];
std::uint64_t KING_ATTACKS[64];
std::uint64_t PAWN_ATTACKS[2][64];
std::uint64_t FILE_BB[8];
std::uint64_t RANK_BB[8];

MagicEntry BISHOP_MAGICS[64];
MagicEntry ROOK_MAGICS[64];
std::uint64_t BISHOP_TABLE[5248];
std::uint64_t ROOK_TABLE[102400];

// ========================================================================
// Known-good magic numbers (from the chess programming community)
// These are pre-computed values that produce collision-free hash tables.
// ========================================================================

static const std::uint64_t ROOK_MAGIC_NUMBERS[64] = {
  0x0080001020400080ULL, 0x0040001000200040ULL, 0x0080081000200080ULL,
  0x0080040800100080ULL, 0x0080020400080080ULL, 0x0080010200040080ULL,
  0x0080008001000200ULL, 0x0080002040800100ULL, 0x0000800020400080ULL,
  0x0000400020005000ULL, 0x0000801000200080ULL, 0x0000800800100080ULL,
  0x0000800400080080ULL, 0x0000800200040080ULL, 0x0000800100020080ULL,
  0x0000800040800100ULL, 0x0000208000400080ULL, 0x0000404000201000ULL,
  0x0000808010002000ULL, 0x0000808008001000ULL, 0x0000808004000800ULL,
  0x0000808002000400ULL, 0x0000010100020004ULL, 0x0000020000408104ULL,
  0x0000208080004000ULL, 0x0000200040005000ULL, 0x0000100080200080ULL,
  0x0000080080100080ULL, 0x0000040080080080ULL, 0x0000020080040080ULL,
  0x0000010080800200ULL, 0x0000800080004100ULL, 0x0000204000800080ULL,
  0x0000200040401000ULL, 0x0000100080802000ULL, 0x0000080080801000ULL,
  0x0000040080800800ULL, 0x0000020080800400ULL, 0x0000020001010004ULL,
  0x0000800040800100ULL, 0x0000204000808000ULL, 0x0000200040008080ULL,
  0x0000100020008080ULL, 0x0000080010008080ULL, 0x0000040008008080ULL,
  0x0000020004008080ULL, 0x0000010002008080ULL, 0x0000004081020004ULL,
  0x0000204000800080ULL, 0x0000200040008080ULL, 0x0000100020008080ULL,
  0x0000080010008080ULL, 0x0000040008008080ULL, 0x0000020004008080ULL,
  0x0000800100020080ULL, 0x0000800041000080ULL, 0x00FFFCDDFCED714AULL,
  0x007FFCDDFCED714AULL, 0x003FFFCDFFD88096ULL, 0x0000040810002101ULL,
  0x0001000204080011ULL, 0x0001000204000801ULL, 0x0001000082000401ULL,
  0x0001FFFAABFAD1A2ULL
};

static const std::uint64_t BISHOP_MAGIC_NUMBERS[64] = {
  0x0002020202020200ULL, 0x0002020202020000ULL, 0x0004010202000000ULL,
  0x0004040080000000ULL, 0x0001104000000000ULL, 0x0000821040000000ULL,
  0x0000410410400000ULL, 0x0000104104104000ULL, 0x0000040404040400ULL,
  0x0000020202020200ULL, 0x0000040102020000ULL, 0x0000040400800000ULL,
  0x0000011040000000ULL, 0x0000008210400000ULL, 0x0000004104104000ULL,
  0x0000002082082000ULL, 0x0004000808080800ULL, 0x0002000404040400ULL,
  0x0001000202020200ULL, 0x0000800802004000ULL, 0x0000800400A00000ULL,
  0x0000200100884000ULL, 0x0000400082082000ULL, 0x0000200041041000ULL,
  0x0002080010101000ULL, 0x0001040008080800ULL, 0x0000208004010400ULL,
  0x0000404004010200ULL, 0x0000840000802000ULL, 0x0000404002011000ULL,
  0x0000808001041000ULL, 0x0000404000820800ULL, 0x0001041000202000ULL,
  0x0000820800101000ULL, 0x0000104400080800ULL, 0x0000020080080080ULL,
  0x0000404040040100ULL, 0x0000808100020100ULL, 0x0001010100020800ULL,
  0x0000808080010400ULL, 0x0000820820004000ULL, 0x0000410410002000ULL,
  0x0000082088001000ULL, 0x0000002011000800ULL, 0x0000080100400400ULL,
  0x0001010101000200ULL, 0x0002020202000400ULL, 0x0001010101000200ULL,
  0x0000410410400000ULL, 0x0000208208200000ULL, 0x0000002084100000ULL,
  0x0000000020880000ULL, 0x0000001002020000ULL, 0x0000040408020000ULL,
  0x0004040404040000ULL, 0x0002020202020000ULL, 0x0000104104104000ULL,
  0x0000002082082000ULL, 0x0000000020841000ULL, 0x0000000000208800ULL,
  0x0000000010020200ULL, 0x0000000404080200ULL, 0x0000040404040400ULL,
  0x0002020202020200ULL
};

// ========================================================================
// Mask generation — relevant occupancy bits for each square
// ========================================================================

// Rook relevant occupancy mask (excludes edge squares on the ray)
static std::uint64_t rookMask(int sq) {
  std::uint64_t mask = 0;
  int r = sq / 8, f = sq % 8;
  // Up
  for (int i = r + 1; i < 7; ++i) mask |= 1ULL << (i * 8 + f);
  // Down
  for (int i = r - 1; i > 0; --i) mask |= 1ULL << (i * 8 + f);
  // Right
  for (int i = f + 1; i < 7; ++i) mask |= 1ULL << (r * 8 + i);
  // Left
  for (int i = f - 1; i > 0; --i) mask |= 1ULL << (r * 8 + i);
  return mask;
}

// Bishop relevant occupancy mask (excludes edge squares on the ray)
static std::uint64_t bishopMask(int sq) {
  std::uint64_t mask = 0;
  int r = sq / 8, f = sq % 8;
  for (int i = 1; r + i < 7 && f + i < 7; ++i) mask |= 1ULL << ((r + i) * 8 + (f + i));
  for (int i = 1; r + i < 7 && f - i > 0; ++i) mask |= 1ULL << ((r + i) * 8 + (f - i));
  for (int i = 1; r - i > 0 && f + i < 7; ++i) mask |= 1ULL << ((r - i) * 8 + (f + i));
  for (int i = 1; r - i > 0 && f - i > 0; ++i) mask |= 1ULL << ((r - i) * 8 + (f - i));
  return mask;
}

// ========================================================================
// Classical ray attack generation (used during initialization only)
// ========================================================================

static std::uint64_t rookAttacksSlow(int sq, std::uint64_t occ) {
  std::uint64_t attacks = 0;
  static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  for (const auto &dir : dirs) {
    int r = sq / 8 + dir[0];
    int f = sq % 8 + dir[1];
    while (r >= 0 && r < 8 && f >= 0 && f < 8) {
      std::uint64_t bit = 1ULL << (r * 8 + f);
      attacks |= bit;
      if (occ & bit) break;
      r += dir[0];
      f += dir[1];
    }
  }
  return attacks;
}

static std::uint64_t bishopAttacksSlow(int sq, std::uint64_t occ) {
  std::uint64_t attacks = 0;
  static const int dirs[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
  for (const auto &dir : dirs) {
    int r = sq / 8 + dir[0];
    int f = sq % 8 + dir[1];
    while (r >= 0 && r < 8 && f >= 0 && f < 8) {
      std::uint64_t bit = 1ULL << (r * 8 + f);
      attacks |= bit;
      if (occ & bit) break;
      r += dir[0];
      f += dir[1];
    }
  }
  return attacks;
}

// ========================================================================
// Occupancy enumeration — generate all subsets of a mask
// ========================================================================

// Given an index (0..2^popcount-1), map it to a subset of the mask
static std::uint64_t indexToOccupancy(int index, int bits, std::uint64_t mask) {
  std::uint64_t occ = 0;
  for (int i = 0; i < bits; ++i) {
    int sq = lsb(mask);
    mask &= mask - 1;
    if (index & (1 << i)) {
      occ |= 1ULL << sq;
    }
  }
  return occ;
}

// ========================================================================
// Standard initialization (knight, king, pawn, file/rank)
// ========================================================================

static void initFilesAndRanks() {
  for (int f = 0; f < 8; ++f) {
    FILE_BB[f] = 0;
    for (int r = 0; r < 8; ++r) {
      FILE_BB[f] |= 1ULL << (r * 8 + f);
    }
  }
  for (int r = 0; r < 8; ++r) {
    RANK_BB[r] = 0;
    for (int f = 0; f < 8; ++f) {
      RANK_BB[r] |= 1ULL << (r * 8 + f);
    }
  }
}

static void initKnightAttacks() {
  static const int offsets[8][2] = {
      {2, 1}, {2, -1}, {-2, 1}, {-2, -1},
      {1, 2}, {1, -2}, {-1, 2}, {-1, -2}};

  for (int sq = 0; sq < 64; ++sq) {
    std::uint64_t attacks = 0;
    int r = sq / 8, f = sq % 8;
    for (const auto &off : offsets) {
      int nr = r + off[0], nf = f + off[1];
      if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
        attacks |= 1ULL << (nr * 8 + nf);
      }
    }
    KNIGHT_ATTACKS[sq] = attacks;
  }
}

static void initKingAttacks() {
  static const int offsets[8][2] = {
      {1, 0}, {1, 1}, {0, 1}, {-1, 1},
      {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};

  for (int sq = 0; sq < 64; ++sq) {
    std::uint64_t attacks = 0;
    int r = sq / 8, f = sq % 8;
    for (const auto &off : offsets) {
      int nr = r + off[0], nf = f + off[1];
      if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
        attacks |= 1ULL << (nr * 8 + nf);
      }
    }
    KING_ATTACKS[sq] = attacks;
  }
}

static void initPawnAttacks() {
  for (int sq = 0; sq < 64; ++sq) {
    int r = sq / 8, f = sq % 8;

    std::uint64_t wAtk = 0;
    if (r < 7) {
      if (f > 0) wAtk |= 1ULL << ((r + 1) * 8 + (f - 1));
      if (f < 7) wAtk |= 1ULL << ((r + 1) * 8 + (f + 1));
    }
    PAWN_ATTACKS[0][sq] = wAtk;

    std::uint64_t bAtk = 0;
    if (r > 0) {
      if (f > 0) bAtk |= 1ULL << ((r - 1) * 8 + (f - 1));
      if (f < 7) bAtk |= 1ULL << ((r - 1) * 8 + (f + 1));
    }
    PAWN_ATTACKS[1][sq] = bAtk;
  }
}

// ========================================================================
// Magic bitboard initialization
// ========================================================================

static void initMagics() {
  // --- Bishop magics ---
  std::uint64_t* bishopPtr = BISHOP_TABLE;
  for (int sq = 0; sq < 64; ++sq) {
    MagicEntry& m = BISHOP_MAGICS[sq];
    m.mask = bishopMask(sq);
    m.magic = BISHOP_MAGIC_NUMBERS[sq];
    int bits = popcount(m.mask);
    m.shift = 64 - bits;
    m.attacks = bishopPtr;

    int numEntries = 1 << bits;

    // Fill the attack table for every possible occupancy
    for (int idx = 0; idx < numEntries; ++idx) {
      std::uint64_t occ = indexToOccupancy(idx, bits, m.mask);
      int tableIdx = static_cast<int>((occ * m.magic) >> m.shift);
      m.attacks[tableIdx] = bishopAttacksSlow(sq, occ);
    }

    bishopPtr += numEntries;
  }

  // --- Rook magics ---
  std::uint64_t* rookPtr = ROOK_TABLE;
  for (int sq = 0; sq < 64; ++sq) {
    MagicEntry& m = ROOK_MAGICS[sq];
    m.mask = rookMask(sq);
    m.magic = ROOK_MAGIC_NUMBERS[sq];
    int bits = popcount(m.mask);
    m.shift = 64 - bits;
    m.attacks = rookPtr;

    int numEntries = 1 << bits;

    for (int idx = 0; idx < numEntries; ++idx) {
      std::uint64_t occ = indexToOccupancy(idx, bits, m.mask);
      int tableIdx = static_cast<int>((occ * m.magic) >> m.shift);
      m.attacks[tableIdx] = rookAttacksSlow(sq, occ);
    }

    rookPtr += numEntries;
  }
}

// ========================================================================
// Main initialization
// ========================================================================

void init() {
  initFilesAndRanks();
  initKnightAttacks();
  initKingAttacks();
  initPawnAttacks();
  initMagics();
}

} // namespace Bitboards
