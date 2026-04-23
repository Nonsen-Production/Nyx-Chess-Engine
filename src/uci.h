#pragma once

#include "board.h"
#include <string>

namespace Chess {
  bool applyUCIMove(Board &board, const std::string &uciMove);
  bool setPositionFromUCI(Board &board, const std::string &uciPositionCommand);
}

void uciLoop();
