# Nyx Chess Engine

Nyx is a **cross-platform**, UCI-compatible chess engine written in C++17. It features a modern search architecture, including alpha-beta pruning, quiescence search, null move pruning, late move reductions, and an efficiently updatable neural network (NNUE) evaluation function.

It natively supports **Windows**, **macOS**, and **Linux**.

## Features

- **Search:** Alpha-beta search with iterative deepening, aspiration windows, check extensions, null move pruning, and LMR.
- **Evaluation:** NNUE (768 → 256 → 32 → 32 → 1) with fallback to hand-crafted evaluation (tapered piece-square tables, pawn structure, king safety, mobility).
- **Move Ordering:** MVV-LVA, killer moves, history heuristic, transposition table move ordering.
- **Transposition Table:** 1M entries hashing with Zobrist keys.
- **Opening Book:** Built-in opening book supporting fundamental chess openings.
- **Cross-Platform Compatibility:** Runs and compiles natively on Windows, macOS, and Linux using CMake.

## Building from Source

Nyx Engine uses CMake for building, ensuring it works effortlessly on any operating system. Ensure you have CMake and a C++17 compatible compiler installed.

### Windows (Visual Studio / MSVC)
1. Clone the repository:
   ```cmd
   git clone https://github.com/Nonsen-Production/Nyx-Chess-Engine.git
   cd Nyx-Engine
   ```
2. Create a build directory and configure:
   ```cmd
   mkdir build
   cd build
   cmake ..
   ```
3. Build the executable:
   ```cmd
   cmake --build . --config Release
   ```
   The executable will be located in `build\Release\nyx.exe`.

### macOS & Linux (GCC / Clang)
1. Clone the repository:
   ```bash
   git clone https://github.com/Nonsen-Production/Nyx-Chess-Engine.git
   cd Nyx-Engine
   ```
2. Create a build directory and configure:
   ```bash
   mkdir build
   cd build
   cmake ..
   ```
3. Build the executable:
   ```bash
   make -j
   ```
   The executable will be located in `build/nyx`.

## Continuous Integration
This project includes GitHub Actions workflows that automatically verify builds on **Ubuntu**, **macOS**, and **Windows** on every push.

## Usage

Nyx supports the standard [Universal Chess Interface (UCI)](https://en.wikipedia.org/wiki/Universal_Chess_Interface) protocol. You can run it directly in the terminal, or connect it to any UCI-compatible GUI such as Arena, Cutechess, or BanksiaGUI.

### NNUE Evaluation

For optimal performance, place the required `nn-nyx.nnue` file in the same working directory where you execute the engine. 

If the NNUE file is missing or not found, Nyx will gracefully fall back to its internal hand-crafted static evaluation. You can explicitly set the evaluation file using the UCI protocol:
```text
setoption name EvalFile value path/to/nn-nyx.nnue
```

### Supported UCI Commands

- `uci` - Identify engine and supported options
- `isready` - Check if the engine is ready
- `ucinewgame` - Start a new game and clear hash tables
- `position startpos [moves ...]` - Set the board to standard starting position
- `position fen <FEN> [moves ...]` - Set the board using a FEN string
- `go depth <depth>` - Begin search up to a certain depth

## License
MIT License
