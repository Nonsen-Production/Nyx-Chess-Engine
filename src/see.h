#pragma once

#include "board.h"
#include "types.h"

namespace Search {

// Static Exchange Evaluation (SEE)
// Returns true if the capture sequence on the target square is winning or equal
// for the side making the first capture.
bool see(const Board& board, const Move& move, int threshold = 0);

// Evaluate the material balance of a capture sequence.
int seeValue(const Board& board, const Move& move);

} // namespace Search
