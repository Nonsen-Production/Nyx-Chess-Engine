# Contributing to Nyx Engine

First off, thank you for considering contributing to Nyx! It's people like you that make this engine better for everyone. We welcome contributions of all kinds, from bug reports and documentation improvements to new features and search enhancements.

## How to Contribute

### 1. Reporting Bugs and Requesting Features
If you find a bug or have an idea for a new feature, please open an issue on our GitHub repository. 
- Provide as much context as possible.
- If reporting a bug, include steps to reproduce the issue, your operating system, and the compiler version you are using.
- If suggesting an engine enhancement (like a new search heuristic or evaluation term), provide any ELO testing data if you have it!

### 2. Development Setup
To start contributing code, you will need to set up your local development environment.

1. **Fork the repository** on GitHub.
2. **Clone your fork** locally:
   ```bash
   git clone https://github.com/YOUR_USERNAME/Nyx-Chess-Engine.git
   cd Nyx-Chess-Engine
   ```
3. **Configure the build** using CMake. Ensure you have CMake and a C++17 compatible compiler installed.
   ```bash
   mkdir build
   cd build
   cmake ..
   ```

### 3. Making Changes
- **Create a new branch** for your feature or bugfix:
  ```bash
  git checkout -b feature/your-feature-name
  ```
- **Write clean, readable C++17 code**. Try to follow the existing coding style in the repository.
- **Keep it modular**. Since we've recently refactored the engine into distinct components (`movegen`, `search`, `eval`, `board`, etc.), make sure your changes go into the appropriate module.

### 4. Testing Your Changes
Before submitting your changes, ensure that:
1. The project compiles successfully on your machine without any warnings.
2. The engine still responds correctly to standard UCI commands (`uci`, `isready`, `position`, `go`).
3. If you've modified the search or evaluation logic, please run a few test positions to ensure the engine doesn't crash or hang.

*Note: Our continuous integration (GitHub Actions) will also test compiling your code across Windows, macOS, and Linux.*

### 5. Submitting a Pull Request
1. Commit your changes with a clear, descriptive commit message:
   ```bash
   git commit -m "Add late move reductions to quiescence search"
   ```
2. Push your branch to your fork on GitHub:
   ```bash
   git push origin feature/your-feature-name
   ```
3. Open a **Pull Request** against the `main` branch of the original repository.
4. In your PR description, explain what changes you made, why you made them, and any testing you performed.

## License
By contributing to Nyx, you agree that your contributions will be licensed under the project's GPL-3 License.
