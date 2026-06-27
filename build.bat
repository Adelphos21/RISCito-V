@echo off
REM Build sencillo en Windows (requiere g++ en el PATH, p.ej. MSYS2 ucrt64).
g++ -std=c++17 -Isrc -O1 -o riscv-sim.exe src\main.cpp src\simulator.cpp src\disasm.cpp
if %errorlevel%==0 echo Compilado: riscv-sim.exe
