#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "disasm.hpp"
#include "simulator.hpp"

// Convierte una cadena a entero sin signo, aceptando prefijo 0x (hex) o decimal.
static bool parse_u32(const std::string& s, uint32_t& out) {
    try {
        size_t pos = 0;
        unsigned long v = std::stoul(s, &pos, 0);
        if (pos != s.size()) return false;
        out = (uint32_t)v;
        return true;
    } catch (...) {
        return false;
    }
}

// Resuelve un nombre de registro ("x5", "5", "sp", "a0") a su índice 0..31.
static int parse_reg(const std::string& s) {
    if (s.empty()) return -1;
    if (s[0] == 'x' || s[0] == 'X') {
        uint32_t n;
        if (parse_u32(s.substr(1), n) && n < 32) return (int)n;
        return -1;
    }
    for (int i = 0; i < 32; ++i)
        if (s == REG_ABI_NAMES[i]) return i;
    uint32_t n;
    if (parse_u32(s, n) && n < 32) return (int)n;
    return -1;
}

static void print_reg(Simulator& sim, int i) {
    printf("x%-2d (%-4s) = 0x%08x  (%d)\n", i, REG_ABI_NAMES[i], sim.get_reg(i),
           (int32_t)sim.get_reg(i));
}

static const char* halt_text(HaltReason r) {
    switch (r) {
        case HaltReason::Ecall:        return "exit por ecall";
        case HaltReason::Ebreak:       return "ebreak";
        case HaltReason::InfiniteLoop: return "bucle infinito (programa terminado)";
        case HaltReason::Error:        return "ERROR de ejecución";
        default:                       return "en ejecución";
    }
}

static void do_step(Simulator& sim, uint32_t n, bool verbose) {
    for (uint32_t i = 0; i < n; ++i) {
        if (sim.is_halted()) {
            printf("CPU detenida (%s). Use 'reset' para reiniciar.\n",
                   halt_text(sim.get_halt_reason()));
            return;
        }
        uint32_t pc = sim.get_pc();
        uint32_t instr = sim.read32(pc);
        if (verbose)
            printf("0x%08x:  %08x   %s\n", pc, instr, disassemble(instr, pc).c_str());
        sim.step();
    }
    if (sim.get_halt_reason() == HaltReason::Error)
        printf("ERROR: %s\n", sim.get_error().c_str());
}

static void print_help() {
    printf(
        "Comandos disponibles:\n"
        "  step [n]            Ejecuta n instrucciones (por defecto 1), mostrando cada una.\n"
        "  run [max]           Ejecuta hasta detenerse (límite opcional, def. 10000000).\n"
        "  pc                  Muestra el contador de programa.\n"
        "  regs [r ...]        Muestra registros (todos, o los indicados: x5, sp, a0...).\n"
        "  setreg <r> <val>    Asigna un valor a un registro.\n"
        "  mem <ini> [fin]     Vuelca memoria en hexadecimal [ini, fin].\n"
        "  set <addr> <w ...>  Escribe words de 32 bits en memoria (precargar datos).\n"
        "  setb <addr> <b ...> Escribe bytes en memoria.\n"
        "  disasm <ini> [fin]  Desensambla instrucciones en el rango.\n"
        "  load <archivo>      Carga un binario crudo en 0x0.\n"
        "  reset               Reinicia PC y registros (conserva la memoria).\n"
        "  help                Muestra esta ayuda.\n"
        "  exit | quit         Sale del simulador.\n");
}

static void cmd_mem(Simulator& sim, uint32_t start, uint32_t end) {
    printf("Memoria (0x%08x-0x%08x):", start, end);
    for (uint32_t a = start; a <= end; ++a) {
        if ((a - start) % 16 == 0) printf("\n  0x%08x: ", a);
        printf("%02x ", sim.read8(a));
    }
    printf("\n");
}

static void cmd_disasm(Simulator& sim, uint32_t start, uint32_t end) {
    for (uint32_t a = start; a <= end; a += 4) {
        uint32_t instr = sim.read32(a);
        printf("0x%08x:  %08x   %s\n", a, instr, disassemble(instr, a).c_str());
    }
}

int main(int argc, char** argv) {
    Simulator sim;
    std::string loaded_name;

    if (argc >= 2) {
        if (sim.load_from_file(argv[1])) {
            loaded_name = argv[1];
            printf("\"%s\" cargado a memoria.\n", argv[1]);
        } else {
            fprintf(stderr, "No se pudo cargar el archivo \"%s\".\n", argv[1]);
            return 1;
        }
    } else {
        printf("Simulador RISC-V (RV32I). Use 'load <archivo>' para cargar un programa.\n");
    }
    printf("Escriba 'help' para ver los comandos.\n");

    std::string line;
    while (true) {
        printf("\n> ");
        fflush(stdout);
        if (!std::getline(std::cin, line)) break;

        std::istringstream iss(line);
        std::string cmd;
        if (!(iss >> cmd)) continue;
        std::vector<std::string> args;
        for (std::string tok; iss >> tok;) args.push_back(tok);

        if (cmd == "exit" || cmd == "quit") {
            break;
        } else if (cmd == "help") {
            print_help();
        } else if (cmd == "pc") {
            printf("pc = 0x%08x\n", sim.get_pc());
        } else if (cmd == "step" || cmd == "s") {
            uint32_t n = 1;
            if (!args.empty()) parse_u32(args[0], n);
            do_step(sim, n, true);
        } else if (cmd == "run" || cmd == "r" || cmd == "continue" || cmd == "c") {
            uint32_t max = 10000000;
            if (!args.empty()) parse_u32(args[0], max);
            uint32_t count = 0;
            while (!sim.is_halted() && count < max) {
                sim.step();
                ++count;
            }
            printf("Ejecutadas %u instrucciones. Estado: %s.\n", count,
                   halt_text(sim.get_halt_reason()));
            if (sim.get_halt_reason() == HaltReason::Error)
                printf("ERROR: %s\n", sim.get_error().c_str());
            else if (!sim.is_halted())
                printf("(límite de instrucciones alcanzado)\n");
        } else if (cmd == "regs" || cmd == "reg") {
            if (args.empty()) {
                for (int i = 0; i < 32; ++i) print_reg(sim, i);
            } else {
                for (auto& a : args) {
                    int r = parse_reg(a);
                    if (r < 0) printf("Registro inválido: %s\n", a.c_str());
                    else print_reg(sim, r);
                }
            }
        } else if (cmd == "setreg") {
            uint32_t v;
            if (args.size() < 2 || parse_reg(args[0]) < 0 || !parse_u32(args[1], v))
                printf("Uso: setreg <registro> <valor>\n");
            else
                sim.set_reg(parse_reg(args[0]), v);
        } else if (cmd == "mem" || cmd == "m") {
            uint32_t start, end;
            if (args.empty() || !parse_u32(args[0], start)) {
                printf("Uso: mem <inicio> [fin]\n");
            } else {
                end = start;
                if (args.size() >= 2) parse_u32(args[1], end);
                if (end < start) end = start;
                cmd_mem(sim, start, end);
            }
        } else if (cmd == "set") {
            uint32_t addr;
            if (args.size() < 2 || !parse_u32(args[0], addr)) {
                printf("Uso: set <addr> <word> [word ...]\n");
            } else {
                for (size_t i = 1; i < args.size(); ++i) {
                    uint32_t w;
                    if (parse_u32(args[i], w)) sim.write32(addr + 4 * (i - 1), w);
                }
                printf("Escritos %zu words desde 0x%08x.\n", args.size() - 1, addr);
            }
        } else if (cmd == "setb") {
            uint32_t addr;
            if (args.size() < 2 || !parse_u32(args[0], addr)) {
                printf("Uso: setb <addr> <byte> [byte ...]\n");
            } else {
                for (size_t i = 1; i < args.size(); ++i) {
                    uint32_t w;
                    if (parse_u32(args[i], w)) sim.write8(addr + (i - 1), w & 0xFF);
                }
                printf("Escritos %zu bytes desde 0x%08x.\n", args.size() - 1, addr);
            }
        } else if (cmd == "disasm" || cmd == "d") {
            uint32_t start, end;
            if (args.empty() || !parse_u32(args[0], start)) {
                printf("Uso: disasm <inicio> [fin]\n");
            } else {
                end = start;
                if (args.size() >= 2) parse_u32(args[1], end);
                if (end < start) end = start;
                cmd_disasm(sim, start, end);
            }
        } else if (cmd == "load") {
            if (args.empty()) {
                printf("Uso: load <archivo>\n");
            } else if (sim.load_from_file(args[0])) {
                loaded_name = args[0];
                printf("\"%s\" cargado a memoria.\n", args[0].c_str());
            } else {
                printf("No se pudo cargar \"%s\".\n", args[0].c_str());
            }
        } else if (cmd == "reset") {
            sim.reset();
            printf("Estado reiniciado (pc = 0, registros = 0).\n");
        } else {
            printf("Comando desconocido: %s. Escriba 'help'.\n", cmd.c_str());
        }
    }

    printf("\nSee you next time...\n");
    return 0;
}
