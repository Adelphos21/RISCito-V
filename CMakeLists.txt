cmake_minimum_required(VERSION 3.11)
project(riscv-simulator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
# -O1: optimizado y estable. (En este toolchain MSYS2, -O2 mostraba fallos
# esporádicos de carga del ejecutable ajenos al simulador.)
set(CMAKE_CXX_FLAGS_RELEASE "-O1")

add_executable(riscv-sim
    src/main.cpp
    src/simulator.cpp
    src/disasm.cpp
)
target_include_directories(riscv-sim PRIVATE src)
