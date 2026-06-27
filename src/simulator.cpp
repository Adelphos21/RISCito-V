#include "simulator.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>

const char* const REG_ABI_NAMES[32] = {
    "zero", "ra", "sp",  "gp",  "tp", "t0", "t1", "t2",
    "s0",   "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
    "a6",   "a7", "s2",  "s3",  "s4", "s5", "s6", "s7",
    "s8",   "s9", "s10", "s11", "t3", "t4", "t5", "t6",
};

// Extiende el signo de un valor de `bits` bits a 32 bits.
static int32_t sign_extend(uint32_t value, int bits) {
    uint32_t m = 1u << (bits - 1);
    return (int32_t)((value ^ m) - m);
}

Simulator::Simulator() : memory(MEMORY_SIZE, 0) {
    reset();
}

void Simulator::reset() {
    pc = 0;
    for (auto& r : regs) r = 0;
    // El stack pointer (x2) arranca en el tope de la memoria.
    regs[2] = MEMORY_SIZE;
    halt_reason = HaltReason::None;
    error_msg.clear();
}

void Simulator::fail(const std::string& msg) {
    halt_reason = HaltReason::Error;
    error_msg = msg;
}

bool Simulator::in_range(uint32_t addr, uint32_t size) const {
    return (uint64_t)addr + size <= MEMORY_SIZE;
}

bool Simulator::load_from_file(const std::string& filename, uint32_t base) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;

    std::vector<char> bytes((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    if (base + bytes.size() > MEMORY_SIZE) {
        std::cerr << "Advertencia: el programa excede la memoria, se trunca.\n";
        bytes.resize(MEMORY_SIZE - base);
    }
    std::fill(memory.begin(), memory.end(), 0);
    std::copy(bytes.begin(), bytes.end(), memory.begin() + base);
    reset();
    return true;
}

// --- Acceso a memoria little-endian ---
uint8_t Simulator::read8(uint32_t addr) {
    if (!in_range(addr, 1)) { fail("lectura fuera de rango en 0x" + std::to_string(addr)); return 0; }
    return memory[addr];
}
uint16_t Simulator::read16(uint32_t addr) {
    if (!in_range(addr, 2)) { fail("lectura fuera de rango en 0x" + std::to_string(addr)); return 0; }
    return memory[addr] | (memory[addr + 1] << 8);
}
uint32_t Simulator::read32(uint32_t addr) {
    if (!in_range(addr, 4)) { fail("lectura fuera de rango en 0x" + std::to_string(addr)); return 0; }
    return memory[addr] | (memory[addr + 1] << 8) | (memory[addr + 2] << 16) |
           ((uint32_t)memory[addr + 3] << 24);
}
void Simulator::write8(uint32_t addr, uint8_t v) {
    if (!in_range(addr, 1)) { fail("escritura fuera de rango en 0x" + std::to_string(addr)); return; }
    memory[addr] = v;
}
void Simulator::write16(uint32_t addr, uint16_t v) {
    if (!in_range(addr, 2)) { fail("escritura fuera de rango en 0x" + std::to_string(addr)); return; }
    memory[addr] = v & 0xFF;
    memory[addr + 1] = (v >> 8) & 0xFF;
}
void Simulator::write32(uint32_t addr, uint32_t v) {
    if (!in_range(addr, 4)) { fail("escritura fuera de rango en 0x" + std::to_string(addr)); return; }
    memory[addr] = v & 0xFF;
    memory[addr + 1] = (v >> 8) & 0xFF;
    memory[addr + 2] = (v >> 16) & 0xFF;
    memory[addr + 3] = (v >> 24) & 0xFF;
}

void Simulator::set_reg(int i, uint32_t v) {
    if (i > 0 && i < 32) regs[i] = v;  // x0 siempre vale 0
}

bool Simulator::step() {
    if (is_halted()) return false;

    if (!in_range(pc, 4)) {
        fail("PC fuera de rango: 0x" + std::to_string(pc));
        return false;
    }

    uint32_t instr = read32(pc);
    execute(instr);

    regs[0] = 0;  // x0 cableado a cero
    return !is_halted();
}

void Simulator::execute(uint32_t instr) {
    uint32_t opcode = instr & 0x7F;
    uint32_t rd     = (instr >> 7) & 0x1F;
    uint32_t funct3 = (instr >> 12) & 0x7;
    uint32_t rs1    = (instr >> 15) & 0x1F;
    uint32_t rs2    = (instr >> 20) & 0x1F;
    uint32_t funct7 = (instr >> 25) & 0x7F;

    // Inmediatos por formato.
    int32_t immI = sign_extend(instr >> 20, 12);
    int32_t immS = sign_extend(((instr >> 25) << 5) | ((instr >> 7) & 0x1F), 12);
    int32_t immB = sign_extend(((instr >> 31) & 1) << 12 | ((instr >> 7) & 1) << 11 |
                                   ((instr >> 25) & 0x3F) << 5 | ((instr >> 8) & 0xF) << 1,
                               13);
    int32_t immU = (int32_t)(instr & 0xFFFFF000);
    int32_t immJ = sign_extend(((instr >> 31) & 1) << 20 | ((instr >> 12) & 0xFF) << 12 |
                                   ((instr >> 20) & 1) << 11 | ((instr >> 21) & 0x3FF) << 1,
                               21);

    uint32_t next_pc = pc + 4;
    uint32_t a = regs[rs1];
    uint32_t b = regs[rs2];

    switch (opcode) {
        case 0x37:  // LUI
            set_reg(rd, (uint32_t)immU);
            break;

        case 0x17:  // AUIPC
            set_reg(rd, pc + (uint32_t)immU);
            break;

        case 0x6F:  // JAL
            set_reg(rd, pc + 4);
            next_pc = pc + (uint32_t)immJ;
            break;

        case 0x67:  // JALR
            if (funct3 == 0) {
                uint32_t target = (a + (uint32_t)immI) & ~1u;
                set_reg(rd, pc + 4);
                next_pc = target;
            } else {
                fail("JALR con funct3 inválido");
            }
            break;

        case 0x63: {  // BRANCH
            bool taken = false;
            switch (funct3) {
                case 0x0: taken = (a == b); break;                          // beq
                case 0x1: taken = (a != b); break;                          // bne
                case 0x4: taken = ((int32_t)a < (int32_t)b); break;         // blt
                case 0x5: taken = ((int32_t)a >= (int32_t)b); break;        // bge
                case 0x6: taken = (a < b); break;                           // bltu
                case 0x7: taken = (a >= b); break;                          // bgeu
                default: fail("branch con funct3 inválido"); return;
            }
            if (taken) next_pc = pc + (uint32_t)immB;
            break;
        }

        case 0x03: {  // LOAD
            uint32_t addr = a + (uint32_t)immI;
            switch (funct3) {
                case 0x0: set_reg(rd, (uint32_t)(int32_t)(int8_t)read8(addr)); break;   // lb
                case 0x1: set_reg(rd, (uint32_t)(int32_t)(int16_t)read16(addr)); break; // lh
                case 0x2: set_reg(rd, read32(addr)); break;                             // lw
                case 0x4: set_reg(rd, read8(addr)); break;                              // lbu
                case 0x5: set_reg(rd, read16(addr)); break;                             // lhu
                default: fail("load con funct3 inválido"); return;
            }
            break;
        }

        case 0x23: {  // STORE
            uint32_t addr = a + (uint32_t)immS;
            switch (funct3) {
                case 0x0: write8(addr, b & 0xFF); break;     // sb
                case 0x1: write16(addr, b & 0xFFFF); break;  // sh
                case 0x2: write32(addr, b); break;           // sw
                default: fail("store con funct3 inválido"); return;
            }
            break;
        }

        case 0x13: {  // OP-IMM
            uint32_t shamt = (instr >> 20) & 0x1F;
            switch (funct3) {
                case 0x0: set_reg(rd, a + (uint32_t)immI); break;                       // addi
                case 0x2: set_reg(rd, (int32_t)a < immI ? 1 : 0); break;                // slti
                case 0x3: set_reg(rd, a < (uint32_t)immI ? 1 : 0); break;               // sltiu
                case 0x4: set_reg(rd, a ^ (uint32_t)immI); break;                       // xori
                case 0x6: set_reg(rd, a | (uint32_t)immI); break;                       // ori
                case 0x7: set_reg(rd, a & (uint32_t)immI); break;                       // andi
                case 0x1: set_reg(rd, a << shamt); break;                               // slli
                case 0x5:
                    if (funct7 == 0x20) set_reg(rd, (uint32_t)((int32_t)a >> shamt));   // srai
                    else                set_reg(rd, a >> shamt);                        // srli
                    break;
            }
            break;
        }

        case 0x33: {  // OP (registro-registro)
            uint32_t shamt = b & 0x1F;
            switch (funct3) {
                case 0x0:
                    if (funct7 == 0x20) set_reg(rd, a - b);  // sub
                    else                set_reg(rd, a + b);  // add
                    break;
                case 0x1: set_reg(rd, a << shamt); break;                               // sll
                case 0x2: set_reg(rd, (int32_t)a < (int32_t)b ? 1 : 0); break;          // slt
                case 0x3: set_reg(rd, a < b ? 1 : 0); break;                            // sltu
                case 0x4: set_reg(rd, a ^ b); break;                                    // xor
                case 0x5:
                    if (funct7 == 0x20) set_reg(rd, (uint32_t)((int32_t)a >> shamt));   // sra
                    else                set_reg(rd, a >> shamt);                        // srl
                    break;
                case 0x6: set_reg(rd, a | b); break;                                    // or
                case 0x7: set_reg(rd, a & b); break;                                    // and
            }
            break;
        }

        case 0x73:  // SYSTEM
            if (instr == 0x00000073)       do_ecall();                  // ecall
            else if (instr == 0x00100073)  halt_reason = HaltReason::Ebreak;  // ebreak
            else fail("instrucción SYSTEM no soportada");
            break;

        case 0x0F:  // FENCE: tratado como nop en este simulador
            break;

        default:
            fail("opcode inválido: 0x" + std::to_string(opcode));
            return;
    }

    // Detección de bucle infinito (p.ej. "done: beq x0,x0,done").
    if (halt_reason == HaltReason::None && next_pc == pc) {
        halt_reason = HaltReason::InfiniteLoop;
        return;
    }

    if (halt_reason == HaltReason::None) pc = next_pc;
}

// Environment calls estilo SPIM (registro a7 = número de servicio).
// Referencia: https://cpulator.01xz.net/doc/#syscall
void Simulator::do_ecall() {
    uint32_t service = regs[17];  // a7
    switch (service) {
        case 1:  // print_int
            std::cout << (int32_t)regs[10];
            break;
        case 4: {  // print_string (a0 = dirección)
            uint32_t addr = regs[10];
            while (in_range(addr, 1) && memory[addr] != 0)
                std::cout << (char)memory[addr++];
            break;
        }
        case 5:  // read_int -> a0
            { int32_t v = 0; std::cin >> v; regs[10] = (uint32_t)v; }
            break;
        case 8: {  // read_string (a0 = buffer, a1 = longitud máx)
            uint32_t addr = regs[10];
            int32_t maxlen = (int32_t)regs[11];
            std::string line;
            std::getline(std::cin, line);
            for (int i = 0; i < maxlen - 1 && i < (int)line.size(); ++i)
                write8(addr + i, line[i]);
            if (maxlen > 0) write8(addr + std::min((int)line.size(), maxlen - 1), 0);
            break;
        }
        case 10:  // exit
            halt_reason = HaltReason::Ecall;
            break;
        case 11:  // print_char
            std::cout << (char)(regs[10] & 0xFF);
            break;
        case 17:  // exit2 (a0 = código de salida)
            halt_reason = HaltReason::Ecall;
            break;
        default:
            fail("ecall no soportada: a7=" + std::to_string(service));
            break;
    }
}
