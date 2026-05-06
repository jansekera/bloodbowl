#!/bin/bash
# Blood Bowl AI — setup script
# Works on: laptop (Ubuntu/Debian) and remote server
# Usage:
#   bash setup.sh          # setup in current directory (must be inside the repo)
#   bash setup.sh /path    # setup in given directory (clones repo there if missing)
set -e

REPO_URL="https://github.com/jansekera/bloodbowl.git"
TARGET_DIR="${1:-$(pwd)}"

echo "=== Blood Bowl AI — Setup ==="
echo "Target: $TARGET_DIR"

# 1. System dependencies
echo ""
echo "[1/4] System dependencies..."
if command -v apt-get &>/dev/null; then
    sudo apt-get update -qq
    sudo apt-get install -y -qq build-essential cmake g++ python3 python3-pip python3-dev python3-venv git
elif command -v dnf &>/dev/null; then
    sudo dnf install -y gcc g++ cmake python3 python3-pip python3-devel git
elif command -v brew &>/dev/null; then
    brew install cmake python3 git
else
    echo "WARN: Unknown package manager — install cmake, g++, python3 manually if build fails"
fi

# 2. Clone repo if not present
if [ ! -f "$TARGET_DIR/bloodbowl_training.ipynb" ]; then
    echo ""
    echo "[2/4] Cloning repository..."
    git clone "$REPO_URL" "$TARGET_DIR"
else
    echo ""
    echo "[2/4] Repository already present, pulling latest..."
    git -C "$TARGET_DIR" pull --rebase origin main || true
fi

cd "$TARGET_DIR"

# 3. Python dependencies
echo ""
echo "[3/4] Python dependencies..."
if [ ! -d "venv" ]; then
    python3 -m venv venv
fi
source venv/bin/activate
pip install -q --upgrade pip
pip install -q pybind11 numpy

# 4. Build C++ engine
echo ""
echo "[4/4] Building C++ engine..."
mkdir -p engine/build

PYBIND_DIR=$(python3 -c "import pybind11; print(pybind11.get_cmake_dir())")
PYTHON_EXE=$(which python3)

cd engine/build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DPYTHON_EXECUTABLE="$PYTHON_EXE" \
    -Dpybind11_DIR="$PYBIND_DIR" \
    -DCMAKE_CXX_FLAGS="-O3" \
    -DBUILD_PYTHON=ON \
    > /dev/null 2>&1
make -j$(nproc) 2>&1 | tail -5
cd ../..

# 5. Verify
echo ""
echo "=== Ověření ==="
echo -n "C++ testy: "
./engine/build/bb_tests --gtest_brief=1 2>&1 | tail -1

echo -n "bb_engine import: "
PYTHONPATH=engine/build:python python3 -c "import bb_engine; r = bb_engine.get_roster('human'); print(f'OK ({r.name})')"

echo ""
echo "=== Setup complete! ==="
echo ""
echo "Spuštění tréninku:"
echo "  source venv/bin/activate"
echo "  python3 run_iteration.py          # 1 iterace"
echo "  python3 run_iteration.py --loop 5 # 5 iterací za sebou"
echo "  python3 run_iteration.py --no-push --loop 5  # bez git push"
