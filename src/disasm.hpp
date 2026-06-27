#ifndef RISCV_DISASM_HPP
#define RISCV_DISASM_HPP

#include <cstdint>
#include <string>

// Devuelve la representación textual de una instrucción RV32I.
// `pc` se usa para resolver objetivos de saltos relativos.
std::string disassemble(uint32_t instr, uint32_t pc);

#endif  // RISCV_DISASM_HPP
