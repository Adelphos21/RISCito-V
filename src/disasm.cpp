#include "disasm.hpp"

#include <cstdarg>
#include <cstdio>

#include "simulator.hpp"  // REG_ABI_NAMES

static int32_t sext(uint32_t value, int bits) {
    uint32_t m = 1u << (bits - 1);
    return (int32_t)((value ^ m) - m);
}

static std::string reg(uint32_t i) { return REG_ABI_NAMES[i & 31]; }

static std::string fmt(const char* f, ...) {
    char buf[128];
    va_list ap;
    va_start(ap, f);
    vsnprintf(buf, sizeof(buf), f, ap);
    va_end(ap);
    return std::string(buf);
}

std::string disassemble(uint32_t instr, uint32_t pc) {
    uint32_t opcode = instr & 0x7F;
    uint32_t rd     = (instr >> 7) & 0x1F;
    uint32_t funct3 = (instr >> 12) & 0x7;
    uint32_t rs1    = (instr >> 15) & 0x1F;
    uint32_t rs2    = (instr >> 20) & 0x1F;
    uint32_t funct7 = (instr >> 25) & 0x7F;

    int32_t immI = sext(instr >> 20, 12);
    int32_t immS = sext(((instr >> 25) << 5) | ((instr >> 7) & 0x1F), 12);
    int32_t immB = sext(((instr >> 31) & 1) << 12 | ((instr >> 7) & 1) << 11 |
                            ((instr >> 25) & 0x3F) << 5 | ((instr >> 8) & 0xF) << 1,
                        13);
    int32_t immU = (int32_t)(instr & 0xFFFFF000);
    int32_t immJ = sext(((instr >> 31) & 1) << 20 | ((instr >> 12) & 0xFF) << 12 |
                            ((instr >> 20) & 1) << 11 | ((instr >> 21) & 0x3FF) << 1,
                        21);
    uint32_t shamt = (instr >> 20) & 0x1F;

    switch (opcode) {
        case 0x37: return fmt("lui %s, 0x%x", reg(rd).c_str(), (uint32_t)immU >> 12);
        case 0x17: return fmt("auipc %s, 0x%x", reg(rd).c_str(), (uint32_t)immU >> 12);
        case 0x6F: return fmt("jal %s, 0x%x", reg(rd).c_str(), pc + immJ);
        case 0x67: return fmt("jalr %s, %d(%s)", reg(rd).c_str(), immI, reg(rs1).c_str());

        case 0x63: {
            const char* m[] = {"beq", "bne", "?", "?", "blt", "bge", "bltu", "bgeu"};
            return fmt("%s %s, %s, 0x%x", m[funct3], reg(rs1).c_str(), reg(rs2).c_str(),
                       pc + immB);
        }
        case 0x03: {
            const char* m[] = {"lb", "lh", "lw", "?", "lbu", "lhu", "?", "?"};
            return fmt("%s %s, %d(%s)", m[funct3], reg(rd).c_str(), immI, reg(rs1).c_str());
        }
        case 0x23: {
            const char* m[] = {"sb", "sh", "sw", "?", "?", "?", "?", "?"};
            return fmt("%s %s, %d(%s)", m[funct3], reg(rs2).c_str(), immS, reg(rs1).c_str());
        }
        case 0x13: {
            switch (funct3) {
                case 0x0: return fmt("addi %s, %s, %d", reg(rd).c_str(), reg(rs1).c_str(), immI);
                case 0x2: return fmt("slti %s, %s, %d", reg(rd).c_str(), reg(rs1).c_str(), immI);
                case 0x3: return fmt("sltiu %s, %s, %d", reg(rd).c_str(), reg(rs1).c_str(), immI);
                case 0x4: return fmt("xori %s, %s, %d", reg(rd).c_str(), reg(rs1).c_str(), immI);
                case 0x6: return fmt("ori %s, %s, %d", reg(rd).c_str(), reg(rs1).c_str(), immI);
                case 0x7: return fmt("andi %s, %s, %d", reg(rd).c_str(), reg(rs1).c_str(), immI);
                case 0x1: return fmt("slli %s, %s, %u", reg(rd).c_str(), reg(rs1).c_str(), shamt);
                case 0x5:
                    return funct7 == 0x20
                               ? fmt("srai %s, %s, %u", reg(rd).c_str(), reg(rs1).c_str(), shamt)
                               : fmt("srli %s, %s, %u", reg(rd).c_str(), reg(rs1).c_str(), shamt);
            }
            break;
        }
        case 0x33: {
            switch (funct3) {
                case 0x0:
                    return funct7 == 0x20
                               ? fmt("sub %s, %s, %s", reg(rd).c_str(), reg(rs1).c_str(), reg(rs2).c_str())
                               : fmt("add %s, %s, %s", reg(rd).c_str(), reg(rs1).c_str(), reg(rs2).c_str());
                case 0x1: return fmt("sll %s, %s, %s", reg(rd).c_str(), reg(rs1).c_str(), reg(rs2).c_str());
                case 0x2: return fmt("slt %s, %s, %s", reg(rd).c_str(), reg(rs1).c_str(), reg(rs2).c_str());
                case 0x3: return fmt("sltu %s, %s, %s", reg(rd).c_str(), reg(rs1).c_str(), reg(rs2).c_str());
                case 0x4: return fmt("xor %s, %s, %s", reg(rd).c_str(), reg(rs1).c_str(), reg(rs2).c_str());
                case 0x5:
                    return funct7 == 0x20
                               ? fmt("sra %s, %s, %s", reg(rd).c_str(), reg(rs1).c_str(), reg(rs2).c_str())
                               : fmt("srl %s, %s, %s", reg(rd).c_str(), reg(rs1).c_str(), reg(rs2).c_str());
                case 0x6: return fmt("or %s, %s, %s", reg(rd).c_str(), reg(rs1).c_str(), reg(rs2).c_str());
                case 0x7: return fmt("and %s, %s, %s", reg(rd).c_str(), reg(rs1).c_str(), reg(rs2).c_str());
            }
            break;
        }
        case 0x73:
            if (instr == 0x00000073) return "ecall";
            if (instr == 0x00100073) return "ebreak";
            return "system";
        case 0x0F: return "fence";
    }
    return fmt(".word 0x%08x", instr);
}
