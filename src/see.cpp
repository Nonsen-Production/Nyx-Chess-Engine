#include "see.h"
#include "bitboard.h"
#include "eval.h"
#include "movegen.h"
#include <algorithm>

namespace Search {

static const int pieceValues[7] = {0, PAWN_VALUE, KNIGHT_VALUE, BISHOP_VALUE, ROOK_VALUE, QUEEN_VALUE, KING_VALUE};

// Get the value of a piece type
static int getPieceValue(int pieceType) {
    if (pieceType >= 1 && pieceType <= 6) return pieceValues[pieceType];
    return 0;
}

// Find the least valuable piece attacking the given square
static int getLeastValuableAttacker(const Board& board, int square, bool whiteToMove, std::uint64_t occupied, std::uint64_t& attackers) {
    // Piece arrays
    int offset = whiteToMove ? 0 : 6;
    
    // Check Pawns
    std::uint64_t pawns = board.pieceBitboards[offset] & attackers;
    if (pawns) {
        int sq = Bitboards::lsb(pawns);
        attackers &= ~(1ULL << sq);
        return 1; // Pawn
    }
    
    // Check Knights
    std::uint64_t knights = board.pieceBitboards[offset + 1] & attackers;
    if (knights) {
        int sq = Bitboards::lsb(knights);
        attackers &= ~(1ULL << sq);
        return 2; // Knight
    }
    
    // Check Bishops
    std::uint64_t bishops = board.pieceBitboards[offset + 2] & attackers;
    if (bishops) {
        int sq = Bitboards::lsb(bishops);
        attackers &= ~(1ULL << sq);
        return 3; // Bishop
    }
    
    // Check Rooks
    std::uint64_t rooks = board.pieceBitboards[offset + 3] & attackers;
    if (rooks) {
        int sq = Bitboards::lsb(rooks);
        attackers &= ~(1ULL << sq);
        return 4; // Rook
    }
    
    // Check Queens
    std::uint64_t queens = board.pieceBitboards[offset + 4] & attackers;
    if (queens) {
        int sq = Bitboards::lsb(queens);
        attackers &= ~(1ULL << sq);
        return 5; // Queen
    }
    
    // Check King
    std::uint64_t king = board.pieceBitboards[offset + 5] & attackers;
    if (king) {
        int sq = Bitboards::lsb(king);
        attackers &= ~(1ULL << sq);
        return 6; // King
    }
    
    return 0; // No attacker found
}

// Generate all attackers to a square, considering x-ray attacks
static std::uint64_t getAttackers(const Board& board, int square, std::uint64_t occupied) {
    std::uint64_t attackers = 0;
    
    // Pawns
    attackers |= Bitboards::PAWN_ATTACKS[1][square] & board.pieceBitboards[0]; // Black pawns attacking square
    attackers |= Bitboards::PAWN_ATTACKS[0][square] & board.pieceBitboards[6]; // White pawns attacking square
    
    // Knights
    attackers |= Bitboards::KNIGHT_ATTACKS[square] & (board.pieceBitboards[1] | board.pieceBitboards[7]);
    
    // Kings
    attackers |= Bitboards::KING_ATTACKS[square] & (board.pieceBitboards[5] | board.pieceBitboards[11]);
    
    // Sliders (Bishops, Rooks, Queens)
    std::uint64_t bishopQueens = board.pieceBitboards[2] | board.pieceBitboards[8] | board.pieceBitboards[4] | board.pieceBitboards[10];
    std::uint64_t rookQueens = board.pieceBitboards[3] | board.pieceBitboards[9] | board.pieceBitboards[4] | board.pieceBitboards[10];
    
    attackers |= Bitboards::bishopAttacks(square, occupied) & bishopQueens;
    attackers |= Bitboards::rookAttacks(square, occupied) & rookQueens;
    
    return attackers;
}

int seeValue(const Board& board, const Move& move) {
    int gain[32];
    int depth = 0;
    
    // Initial capture
    int capturedType = std::abs(board.sqaures[move.to]);
    if (move.promotion != 0) {
        // Assume promotion increases value of the piece
        capturedType += std::abs(move.promotion) - 1; 
    }
    
    gain[depth] = getPieceValue(capturedType);
    
    int currentPieceType = std::abs(board.sqaures[move.from]);
    if (move.promotion != 0) currentPieceType = std::abs(move.promotion);
    
    std::uint64_t occupied = (board.whitePieces | board.blackPieces);
    
    // Make the initial move on the occupied bitboard
    occupied &= ~(1ULL << move.from);
    occupied |= (1ULL << move.to);
    
    bool whiteTurn = !board.whiteTurn;
    
    // Get all attackers to the destination square
    std::uint64_t attackers = getAttackers(board, move.to, occupied);
    
    // Simulate the sequence of captures
    while (true) {
        depth++;
        
        // Find the least valuable attacker for the current side
        int nextAttackerType = getLeastValuableAttacker(board, move.to, whiteTurn, occupied, attackers);
        
        if (nextAttackerType == 0) break; // No more captures
        
        // The gain is the value of the piece just captured minus the value of our piece that might be captured next
        gain[depth] = getPieceValue(currentPieceType) - gain[depth - 1];
        
        // If the new gain is negative, the side to move wouldn't make the capture
        if (std::max(-gain[depth - 1], gain[depth]) < 0) break;
        
        // Update state for next iteration
        currentPieceType = nextAttackerType;
        whiteTurn = !whiteTurn;
        
        // When a piece moves, it might reveal an x-ray attack
        // To handle this perfectly we should re-compute attackers or use x-ray bitboards.
        // For simplicity, we just recompute sliders if the last attacker was not a knight/king
        if (nextAttackerType != 2 && nextAttackerType != 6) {
             std::uint64_t bishopQueens = board.pieceBitboards[2] | board.pieceBitboards[8] | board.pieceBitboards[4] | board.pieceBitboards[10];
             std::uint64_t rookQueens = board.pieceBitboards[3] | board.pieceBitboards[9] | board.pieceBitboards[4] | board.pieceBitboards[10];
             std::uint64_t newAttackers = Bitboards::bishopAttacks(move.to, occupied) & bishopQueens;
             newAttackers |= Bitboards::rookAttacks(move.to, occupied) & rookQueens;
             attackers |= newAttackers & ~occupied; // Add newly revealed attackers
        }
    }
    
    // Minimax the gain array to find the final value
    while (--depth > 0) {
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
    }
    
    return gain[0];
}

bool see(const Board& board, const Move& move, int threshold) {
    return seeValue(board, move) >= threshold;
}

} // namespace Search
