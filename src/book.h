#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

/*
 * Opening Book for Nyx Chess Engine
 *
 * Supports two modes:
 * 1. Built-in book: hardcoded strong opening lines
 * 2. Polyglot .bin reader (future extension)
 *
 * The built-in book stores common opening sequences as UCI move strings.
 * At each position, we track which moves in the game have been played
 * and look up the next book move from matching lines.
 */

namespace Book {

struct BookLine {
  const char* moves; // Space-separated UCI moves
  int weight;        // Selection weight (higher = preferred)
};

// Built-in opening book: strong, well-known opening lines
// Each line is a space-separated sequence of UCI moves from startpos
static const BookLine BUILTIN_BOOK[] = {
    // === KING'S PAWN OPENINGS (1.e4) ===

    // Ruy Lopez (Main Line / Morphy Defense)
    {"e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f8e7 f1e1 b7b5 a4b3 "
     "d7d6 c2c3 e8g8 h2h3",
     100},
    // Ruy Lopez (Berlin Defense)
    {"e2e4 e7e5 g1f3 b8c6 f1b5 g8f6 e1g1 f6e4 d2d4 b8d6 b5c6 d7c6 d4e5 "
     "d6f5 d1d8 e8d8",
     80},
    // Ruy Lopez (Marshall Attack)
    {"e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f8e7 f1e1 b7b5 a4b3 "
     "e8g8 c2c3 d7d5 e4d5 f6d5 f3e5",
     70},     
    // Italian Game (Giuoco Piano)
    {"e2e4 e7e5 g1f3 b8c6 f1c4 f8c5 c2c3 g8f6 d2d4 e5d4 c3d4 c5b4 "
     "b1c3",
     90},
    // Italian Game (Two Knights)
    {"e2e4 e7e5 g1f3 b8c6 f1c4 g8f6 d2d4 e5d4 e1g1 f6e4 f1e1", 70},
    {"e2e4 e7e5 g1f3 b8c6 f1c4 g8f6 f3g5 d7d5 e4d5 c6a5", 60},
    // Scotch Game
    {"e2e4 e7e5 g1f3 b8c6 d2d4 e5d4 f3d4 g8f6 b1c3 f8b4", 70},
    {"e2e4 e7e5 g1f3 b8c6 d2d4 e5d4 f3d4 f8c5 c1e3", 60},
    // Four Knights
    {"e2e4 e7e5 g1f3 b8c6 b1c3 g8f6 f1b5 f8b4", 60},
    // Petroff Defense
    {"e2e4 e7e5 g1f3 g8f6 f3e5 d7d6 e5f3 f6e4 d2d4", 70},
    {"e2e4 e7e5 g1f3 g8f6 d2d4 f6e4 f1d3 d7d5", 60},
    // Philidor Defense
    {"e2e4 e7e5 g1f3 d7d6 d2d4 g8f6 d4e5 f6e4", 40},

    // Sicilian Defense (Open / Najdorf)
    {"e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 a7a6 c1e3", 100},
    {"e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 a7a6 c1g5", 90},
    // Sicilian (Dragon)
    {"e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 g7g6 c1e3 f8g7 f2f3", 80},
    // Sicilian (Classical / Scheveningen)
    {"e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 e7e6 g2g4", 80},
    // Sicilian (Sveshnikov)
    {"e2e4 c7c5 g1f3 b8c6 d2d4 c5d4 f3d4 g8f6 b1c3 e7e5 d4b5", 80},
    // Sicilian (Taimanov)
    {"e2e4 c7c5 g1f3 e7e6 d2d4 c5d4 f3d4 b8c6 b1c3 d8c7", 70},
    // Sicilian (Alapin)
    {"e2e4 c7c5 c2c3 d7d5 e4d5 d8d5 d2d4 g8f6 g1f3 c8g4", 60},
    // Sicilian (Closed)
    {"e2e4 c7c5 b1c3 b8c6 g2g3 g7g6 f1g2 f8g7", 50},

    // French Defense (Winawer)
    {"e2e4 e7e6 d2d4 d7d5 b1c3 f8b4 e4e5 c7c5 a2a3", 80},
    // French Defense (Tarrasch)
    {"e2e4 e7e6 d2d4 d7d5 b1d2 g8f6 e4e5 f6d7 f1d3 c7c5", 70},
    // French Defense (Advance)
    {"e2e4 e7e6 d2d4 d7d5 e4e5 c7c5 c2c3 b8c6 g1f3", 70},

    // Caro-Kann Defense
    {"e2e4 c7c6 d2d4 d7d5 b1c3 d5e4 c3e4 c8f5 e4g3 f5g6", 80},
    {"e2e4 c7c6 d2d4 d7d5 b1c3 d5e4 c3e4 b8d7 g1f3 g8f6 e4f6", 70},
    {"e2e4 c7c6 d2d4 d7d5 e4e5 c8f5 g1f3 e7e6 f1e2", 70},

    // Pirc / Modern
    {"e2e4 d7d6 d2d4 g8f6 b1c3 g7g6 f1c4 f8g7 d1e2", 50},
    {"e2e4 g7g6 d2d4 f8g7 b1c3 c7c6 h2h3 d7d5", 50},

    // Scandinavian
    {"e2e4 d7d5 e4d5 d8d5 b1c3 d5a5 d2d4 g8f6 g1f3", 50},
    {"e2e4 d7d5 e4d5 g8f6 d2d4 f6d5 g1f3", 40},

    // Alekhine's Defense
    {"e2e4 g8f6 e4e5 f6d5 d2d4 d7d6 g1f3 d6e5", 40},

    // === QUEEN'S PAWN OPENINGS (1.d4) ===

    // Queen's Gambit Declined
    {"d2d4 d7d5 c2c4 e7e6 b1c3 g8f6 c1g5 f8e7 e2e3 e8g8 g1f3 b8d7", 100},
    {"d2d4 d7d5 c2c4 e7e6 b1c3 g8f6 c4d5 e6d5", 80}, // Exchange
    // Queen's Gambit Accepted
    {"d2d4 d7d5 c2c4 d5c4 g1f3 g8f6 e2e3 e7e6 f1c4 c7c5 e1g1", 80},
    // Slav Defense
    {"d2d4 d7d5 c2c4 c7c6 g1f3 g8f6 b1c3 d5c4 a2a4 c8f5 e2e3", 80},
    {"d2d4 d7d5 c2c4 c7c6 b1c3 g8f6 e2e3 a7a6", 70}, // Chebanenko
    // Semi-Slav
    {"d2d4 d7d5 c2c4 c7c6 g1f3 g8f6 b1c3 e7e6 e2e3 b8d7 f1d3", 70},
    {"d2d4 d7d5 c2c4 c7c6 g1f3 g8f6 b1c3 e7e6 c1g5 d4c4", 60}, // Botvinnik

    // King's Indian Defense
    {"d2d4 g8f6 c2c4 g7g6 b1c3 f8g7 e2e4 d7d6 g1f3 e8g8 f1e2 e7e5", 90},
    {"d2d4 g8f6 c2c4 g7g6 b1c3 f8g7 e2e4 d7d6 f2f3 e8g8 c1e3", 70},

    // Nimzo-Indian Defense
    {"d2d4 g8f6 c2c4 e7e6 b1c3 f8b4 d1c2 e8g8 a2a3 b4c3 c2c3", 80},
    {"d2d4 g8f6 c2c4 e7e6 b1c3 f8b4 e2e3 e8g8 f1d3 d7d5 g1f3", 70},

    // Queen's Indian Defense
    {"d2d4 g8f6 c2c4 e7e6 g1f3 b7b6 g2g3 c8b7 f1g2 f8e7 e1g1", 70},

    // Bogo-Indian Defense
    {"d2d4 g8f6 c2c4 e7e6 g1f3 f8b4 c1d2", 60},

    // Grünfeld Defense
    {"d2d4 g8f6 c2c4 g7g6 b1c3 d7d5 c4d5 f6d5 e2e4 d5c3 b2c3 f8g7", 70},

    // Catalan Opening
    {"d2d4 g8f6 c2c4 e7e6 g2g3 d7d5 f1g2 f8e7 g1f3 e8g8 e1g1", 80},

    // London System
    {"d2d4 d7d5 c1f4 g8f6 e2e3 e7e6 g1f3 f8d6 f4g3", 60},
    {"d2d4 g8f6 c1f4 d7d5 e2e3 e7e6 g1f3 f8d6 f4g3 e8g8", 60},

    // Dutch Defense
    {"d2d4 f7f5 g2g3 g8f6 f1g2 g7g6", 50},

    // Benoni Defense
    {"d2d4 g8f6 c2c4 c7c5 d4d5 e7e6 b1c3 e6d5 c4d5", 50},

    // === FLANK OPENINGS ===

    // English Opening
    {"c2c4 e7e5 b1c3 g8f6 g1f3 b8c6 g2g3 d7d5 c4d5 f6d5", 70},
    {"c2c4 c7c5 b1c3 b8c6 g2g3 g7g6 f1g2 f8g7 g1f3", 50},

    // Réti Opening
    {"g1f3 d7d5 g2g3 g8f6 f1g2 g7g6 e1g1 f8g7 d2d3", 50},

    // King's Indian Attack
    {"g1f3 d7d5 g2g3 g8f6 f1g2 e7e6 e1g1 f8e7 d2d3 e8g8", 50},
};

static constexpr int BUILTIN_BOOK_SIZE =
    sizeof(BUILTIN_BOOK) / sizeof(BUILTIN_BOOK[0]);

// State
struct BookState {
  bool useBook = true;
  bool loaded = false;
};

extern BookState gBookState;

// Initialize the book system
void init();

// Probe the book for the current position.
// gameMovesUCI: space-separated UCI moves played from startpos
// Returns a UCI move string if found, empty string if not in book.
std::string probe(const std::string& gameMovesUCI);

// Enable/disable book usage
void setEnabled(bool enabled);
bool isEnabled();

} // namespace Book
