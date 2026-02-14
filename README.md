## GNSS LOGGER

### Hardware
* M5Stack Core2
* M5Stack GNSS Module (NEO-M9N-00)
  * https://docs.m5stack.com/en/module/GNSS%20Module

### Environment
* VSCode
* PlatformIO IDE

### Usage
* SparkFun_u-blox_GNSS_Arduino_Library(https://github.com/sparkfun/SparkFun_u-blox_GNSS_Arduino_Library)

### Code Formatting

This project uses `clang-format` for consistent code style. The formatting is automatically checked on every push and pull request via GitHub Actions.

#### Formatting Scope:
The formatter uses a whitelist approach and only formats files in the following directories:
- `m5GnssLogger/src/` - Source files
- `m5GnssLogger/include/` - Header files
- `m5GnssLogger/test/` - Test files

Files in dependency directories (`.pio/`, `.vscode/`, `build/`, etc.) are excluded from formatting.

#### Files Created:
- `.clang-format` - Configuration file for code style
- `tools/format.sh` - Script to format C++ source files (whitelist approach)
- `tools/Dockerfile` - Docker image with clang-format
- `tools/run-formatter-on-docker.sh` - Convenience script for local Docker-based formatting
- `.github/workflows/format.yml` - GitHub Actions workflow

#### Local Usage:

1. **Using the convenience script (recommended):**
   ```bash
   # Check formatting without modifying files (dry-run, default)
   ./tools/run-formatter-on-docker.sh
   
   # Apply formatting to all files
   ./tools/run-formatter-on-docker.sh -fix
   ```

2. **Using Docker directly:**
   ```bash
   # Build the Docker image
   docker build -t clang-format-check ./tools
   
   # Format all files
   docker run --rm -v "$(pwd):/workspace" clang-format-check
   
   # Check formatting without modifying files (dry-run)
   docker run --rm -e DRY_RUN=true -v "$(pwd):/workspace" clang-format-check
   ```

3. **Using clang-format directly:**
   ```bash
   # Install clang-format on your system
   # Then run:
   find . -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | xargs clang-format -i -style=file
   ```
