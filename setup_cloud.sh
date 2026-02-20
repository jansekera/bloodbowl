#!/bin/bash
# Blood Bowl AI - Cloud setup script
# Tested on: Ubuntu 22.04+ (x86_64 / ARM64)
# Usage: bash setup_cloud.sh
set -e

echo "=== Blood Bowl AI - Cloud Setup ==="

# 1. System dependencies
echo "Installing system dependencies..."
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake g++ \
    python3 python3-pip python3-dev python3-venv \
    git

# 2. Clone repo
if [ ! -d "bloodbowl" ]; then
    echo "Cloning repository..."
    git clone https://github.com/jansekera/bloodbowl.git
fi
cd bloodbowl

# 3. Python dependencies
echo "Setting up Python..."
python3 -m venv venv
source venv/bin/activate
pip install numpy pybind11

# 4. Build C++ engine with pybind11
echo "Building C++ engine..."
mkdir -p engine/build
cd engine/build

PYBIND_INCLUDE=$(python3 -c "import pybind11; print(pybind11.get_cmake_dir())")
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DPYTHON_EXECUTABLE=$(which python3) \
    -Dpybind11_DIR="$PYBIND_INCLUDE"
make -j$(nproc)

cd ../..

# 5. Verify
echo ""
echo "=== Verification ==="
echo "C++ tests:"
./engine/build/bb_tests --gtest_brief=1 2>&1 | tail -3

echo ""
echo "Python binding:"
PYTHONPATH=engine/build:python python3 -c "import bb_engine; print(f'bb_engine loaded: {len(bb_engine.get_all_roster_names())} rosters')"

echo ""
echo "=== Setup complete! ==="
echo ""
echo "To start training:"
echo "  source venv/bin/activate"
echo "  PYTHONPATH=engine/build:python python3 -m blood_bowl.train_cli \\"
echo "    --epochs=10 --games=20 --use-cpp \\"
echo "    --opponent=greedy --home-race=human --away-race=orc \\"
echo "    --mcts-iterations=400 --policy-lr=0.01 \\"
echo "    --lr=0.0 --model=linear \\"
echo "    --weights=weights_best.json \\"
echo "    --training-method=mc_shaped --gamma=0.99 \\"
echo "    --epsilon-start=0.15 --epsilon-end=0.05 \\"
echo "    --benchmark-interval=5 --benchmark-matches=15 \\"
echo "    --skip-greedy-benchmark --timeout=300"
