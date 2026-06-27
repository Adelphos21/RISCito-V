#ifndef RISCV_SIMULATOR_HPP
#define RISCV_SIMULATOR_HPP

#include <cstdint>
#include <string>
#include <vector>

// Tamaño de la memoria plana del simulador (1 MiB).
// Cubre el programa (0x0), los datos de prueba (0x64, 0x1000) y el stack,
// que crece hacia abajo desde el tope de la memoria.
constexpr uint32_t MEMORY_SIZE = 0x100000;  // 1 MiB

// Nombres ABI de los 32 registros (x0..x31).
extern const char* const REG_ABI_NAMES[32];

// Razón por la que la ejecución se detuvo.
enum class HaltReason {
    None,        // sigue corriendo
    Ecall,       // ecall de salida (exit)
    Ebreak,      // instrucción ebreak
    InfiniteLoop, // salto a sí mismo (p.ej. "done: beq x0,x0,done")
    Error,       // error de ejecución (instrucción inválida, acceso fuera de rango)
};

class Simulator {
public:
    Simulator();

    // Carga un binario crudo en memoria a partir del address base (por defecto 0).
    // Reinicia el estado de la CPU. Devuelve false si el archivo no se pudo abrir.
    bool load_from_file(const std::string& filename, uint32_t base = 0);

    // Reinicia PC, registros y razón de parada (no borra la memoria).
    void reset();

    // Ejecuta una sola instrucción. Devuelve false si la CPU quedó detenida.
    bool step();

    // --- Inspección de estado ---
    uint32_t get_pc() const { return pc; }
    uint32_t get_reg(int i) const { return regs[i & 31]; }
    bool is_halted() const { return halt_reason != HaltReason::None; }
    HaltReason get_halt_reason() const { return halt_reason; }
    const std::string& get_error() const { return error_msg; }

    // --- Acceso a memoria (little-endian) con verificación de rango ---
    uint8_t  read8(uint32_t addr);
    uint16_t read16(uint32_t addr);
    uint32_t read32(uint32_t addr);
    void write8(uint32_t addr, uint8_t v);
    void write16(uint32_t addr, uint16_t v);
    void write32(uint32_t addr, uint32_t v);

    // Escritura directa de un registro (para inspección/edición desde el REPL).
    void set_reg(int i, uint32_t v);

private:
    uint32_t pc = 0;
    uint32_t regs[32] = {};
    std::vector<uint8_t> memory;
    HaltReason halt_reason = HaltReason::None;
    std::string error_msg;

    void execute(uint32_t instr);
    void do_ecall();
    void fail(const std::string& msg);
    bool in_range(uint32_t addr, uint32_t size) const;
};

#endif  // RISCV_SIMULATOR_HPP
