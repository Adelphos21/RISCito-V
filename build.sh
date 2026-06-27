#!/usr/bin/env bash
# Build sencillo sin necesidad de make/ninja.
set -e
g++ -std=c++17 -Isrc -O1 -o riscv-sim src/main.cpp src/simulator.cpp src/disasm.cpp
echo "Compilado: ./riscv-sim"
