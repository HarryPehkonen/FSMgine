#!/usr/bin/env bash
# Build documentation locally for FSMgine

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Building FSMgine documentation...${NC}"

# Check if Doxygen is installed
if ! command -v doxygen &> /dev/null; then
    echo -e "${RED}Error: Doxygen is not installed.${NC}"
    echo "Please install Doxygen:"
    echo "  Ubuntu/Debian: sudo apt-get install doxygen graphviz"
    echo "  macOS:         brew install doxygen graphviz"
    echo "  Other:         https://www.doxygen.nl/download.html"
    exit 1
fi

# Check if we're in the project root
if [ ! -f "Doxyfile" ]; then
    echo -e "${RED}Error: Doxyfile not found.${NC}"
    echo "Please run this script from the FSMgine project root directory."
    exit 1
fi

# Build documentation
echo -e "${YELLOW}Running Doxygen...${NC}"
doxygen Doxyfile

if [ $? -eq 0 ]; then
    echo -e "${GREEN}Documentation built successfully!${NC}"
    echo "Documentation is available at: docs/html/index.html"
    
    # Offer to open in browser
    if command -v xdg-open &> /dev/null; then
        read -p "Open documentation in browser? (y/n) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            xdg-open docs/html/index.html
        fi
    elif command -v open &> /dev/null; then
        read -p "Open documentation in browser? (y/n) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            open docs/html/index.html
        fi
    fi
else
    echo -e "${RED}Error: Documentation build failed.${NC}"
    exit 1
fi 