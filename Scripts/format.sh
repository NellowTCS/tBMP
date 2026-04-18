#!/bin/bash
# Format all JS/TS files using Prettier and all C/C++ files using clang-format

set -e

# Format JS/TS in Demo/ and Docs/ using npm script
echo "Formatting JS/TS files with Prettier (via npm)..."
cd Demo && npm run format && cd ..

# Format C/C++ in src/, include/, tools/, and tests/
echo "Formatting C/C++ files with clang-format..."
find src include tools tests -type f \( -name '*.c' -o -name '*.h' -o -name '*.cpp' -o -name '*.hpp' \) -exec clang-format -i {} +

echo "Formatting complete."
