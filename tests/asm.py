#!/usr/bin/env python3
"""Mini-ensamblador RV32I -> binario crudo little-endian.

NO es parte del simulador: es una utilidad para generar binarios de prueba
reproducibles (equivalentes a los que se descargarían de CPUlator en formato
Raw), de modo que el simulador pueda verificarse de forma automática.

Uso:  python asm.py entrada.s salida.bin
"""
import sys
import struct
import re

LABEL_RE = re.compile(r"^([A-Za-z_.$][\w.$]*)\s*:\s*")

# Registros: nombres ABI e índices x0..x31.
ABI = ["zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
       "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
       "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
       "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"]
ALIAS = {"fp": 8}


def reg(tok):
    tok = tok.strip().rstrip(",")
    if tok in ABI:
        return ABI.index(tok)
    if tok in ALIAS:
        return ALIAS[tok]
    if tok and tok[0] == "x" and tok[1:].isdigit():
        n = int(tok[1:])
        if 0 <= n < 32:
            return n
    raise ValueError(f"registro inválido: {tok}")


def imm(tok, labels=None, cur=0, rel=False):
    tok = tok.strip().rstrip(",")
    if labels is not None and tok in labels:
        return labels[tok] - cur if rel else labels[tok]
    return int(tok, 0)


# Codificadores por formato.
def R(f7, f3, op):
    return lambda a, lb, pc: (f7 << 25) | (reg(a[2]) << 20) | (reg(a[1]) << 15) | (f3 << 12) | (reg(a[0]) << 7) | op

def Iar(f3, op):
    return lambda a, lb, pc: ((imm(a[2], lb, pc) & 0xFFF) << 20) | (reg(a[1]) << 15) | (f3 << 12) | (reg(a[0]) << 7) | op

def Ish(f7, f3, op):
    return lambda a, lb, pc: (f7 << 25) | ((imm(a[2], lb, pc) & 0x1F) << 20) | (reg(a[1]) << 15) | (f3 << 12) | (reg(a[0]) << 7) | op


def parse_mem(tok2, tok3):
    """Formato  off(rs1)  -> (off, rs1).  Acepta 'off(rs1)' partido en 1 o 2 tokens."""
    s = (tok2 + tok3).replace(",", "")
    off, rest = s.split("(")
    rs1 = rest.rstrip(")")
    return off, rs1


def Ild(f3, op):
    def enc(a, lb, pc):
        off, rs1 = parse_mem(a[1], a[2] if len(a) > 2 else "")
        return ((imm(off, lb, pc) & 0xFFF) << 20) | (reg(rs1) << 15) | (f3 << 12) | (reg(a[0]) << 7) | op
    return enc

def S(f3, op):
    def enc(a, lb, pc):
        off, rs1 = parse_mem(a[1], a[2] if len(a) > 2 else "")
        v = imm(off, lb, pc) & 0xFFF
        return (((v >> 5) & 0x7F) << 25) | (reg(a[0]) << 20) | (reg(rs1) << 15) | (f3 << 12) | ((v & 0x1F) << 7) | op
    return enc

def B(f3, op):
    def enc(a, lb, pc):
        v = imm(a[2], lb, pc, rel=True) & 0x1FFF
        b12 = (v >> 12) & 1; b11 = (v >> 11) & 1
        b10_5 = (v >> 5) & 0x3F; b4_1 = (v >> 1) & 0xF
        return (b12 << 31) | (b10_5 << 25) | (reg(a[1]) << 20) | (reg(a[0]) << 15) | (f3 << 12) | (b4_1 << 8) | (b11 << 7) | op
    return enc

def U(op):
    return lambda a, lb, pc: ((imm(a[1], lb, pc) & 0xFFFFF) << 12) | (reg(a[0]) << 7) | op

def J(op):
    def enc(a, lb, pc):
        v = imm(a[1], lb, pc, rel=True) & 0x1FFFFF
        b20 = (v >> 20) & 1; b10_1 = (v >> 1) & 0x3FF
        b11 = (v >> 11) & 1; b19_12 = (v >> 12) & 0xFF
        return (b20 << 31) | (b10_1 << 21) | (b11 << 20) | (b19_12 << 12) | (reg(a[0]) << 7) | op
    return enc

def Jalr(a, lb, pc):
    off, rs1 = parse_mem(a[1], a[2] if len(a) > 2 else "")
    return ((imm(off, lb, pc) & 0xFFF) << 20) | (reg(rs1) << 15) | (0 << 12) | (reg(a[0]) << 7) | 0x67


# Tabla de instrucciones reales: nombre -> (encoder, num_operandos esperado).
OPS = {
    "add": R(0x00, 0x0, 0x33), "sub": R(0x20, 0x0, 0x33),
    "sll": R(0x00, 0x1, 0x33), "slt": R(0x00, 0x2, 0x33),
    "sltu": R(0x00, 0x3, 0x33), "xor": R(0x00, 0x4, 0x33),
    "srl": R(0x00, 0x5, 0x33), "sra": R(0x20, 0x5, 0x33),
    "or": R(0x00, 0x6, 0x33), "and": R(0x00, 0x7, 0x33),
    "addi": Iar(0x0, 0x13), "slti": Iar(0x2, 0x13), "sltiu": Iar(0x3, 0x13),
    "xori": Iar(0x4, 0x13), "ori": Iar(0x6, 0x13), "andi": Iar(0x7, 0x13),
    "slli": Ish(0x00, 0x1, 0x13), "srli": Ish(0x00, 0x5, 0x13), "srai": Ish(0x20, 0x5, 0x13),
    "lb": Ild(0x0, 0x03), "lh": Ild(0x1, 0x03), "lw": Ild(0x2, 0x03),
    "lbu": Ild(0x4, 0x03), "lhu": Ild(0x5, 0x03),
    "sb": S(0x0, 0x23), "sh": S(0x1, 0x23), "sw": S(0x2, 0x23),
    "beq": B(0x0, 0x63), "bne": B(0x1, 0x63), "blt": B(0x4, 0x63),
    "bge": B(0x5, 0x63), "bltu": B(0x6, 0x63), "bgeu": B(0x7, 0x63),
    "lui": U(0x37), "auipc": U(0x17),
    "jal": J(0x6F), "jalr": Jalr,
    "ecall": lambda a, lb, pc: 0x73, "ebreak": lambda a, lb, pc: 0x100073,
}


def expand(mn, ops):
    """Expande pseudo-instrucciones a instrucciones reales (lista de (mn, ops))."""
    if mn == "nop":
        return [("addi", ["zero", "zero", "0"])]
    if mn == "mv":
        return [("addi", [ops[0], ops[1], "0"])]
    if mn == "not":
        return [("xori", [ops[0], ops[1], "-1"])]
    if mn == "neg":
        return [("sub", [ops[0], "zero", ops[1]])]
    if mn == "seqz":
        return [("sltiu", [ops[0], ops[1], "1"])]
    if mn == "snez":
        return [("sltu", [ops[0], "zero", ops[1]])]
    if mn == "j":
        return [("jal", ["zero", ops[0]])]
    if mn == "jr":
        return [("jalr", ["zero", "0(" + ops[0] + ")"])]
    if mn == "ret":
        return [("jalr", ["zero", "0(ra)"])]
    if mn == "call":
        return [("jal", ["ra", ops[0]])]
    if mn == "beqz":
        return [("beq", [ops[0], "zero", ops[1]])]
    if mn == "bnez":
        return [("bne", [ops[0], "zero", ops[1]])]
    if mn == "bgt":
        return [("blt", [ops[1], ops[0], ops[2]])]
    if mn == "ble":
        return [("bge", [ops[1], ops[0], ops[2]])]
    if mn == "li" or mn == "la":
        # Resuelto en pass2 (depende del valor); marcador especial.
        return [("__li__", ops)]
    return [(mn, ops)]


# Un 'li'/'la' reserva 1 word si es una etiqueta-marcador resuelta luego como
# valor inmediato pequeño, o hasta 2 (lui+addi). Reservamos siempre 2 cuando el
# operando es una etiqueta (caso peor) y rellenamos con nop lo que sobre.
def li_reserved(tok):
    if _is_int(tok):
        sval = int(tok, 0)
        sval = sval if sval < 0x80000000 else sval - 0x100000000
        return 1 if -2048 <= sval <= 2047 else 2
    return 2  # etiqueta: reservar lui+addi


def encode_li(rd, val):
    """Devuelve la lista de words (1 o 2) que materializan rd = val."""
    val &= 0xFFFFFFFF
    sval = val if val < 0x80000000 else val - 0x100000000
    if -2048 <= sval <= 2047:
        return [((val & 0xFFF) << 20) | (rd << 7) | 0x13]  # addi rd, zero, val
    lo = val & 0xFFF
    hi = (val - (lo - 0x1000 if lo >= 0x800 else lo)) & 0xFFFFFFFF
    out = [((hi >> 12) << 12) | (rd << 7) | 0x37]  # lui rd, hi
    if lo:
        out.append(((lo & 0xFFF) << 20) | (rd << 15) | (rd << 7) | 0x13)  # addi rd, rd, lo
    return out


def assemble(src):
    # --- Tokenización en lista de items (label / instr / dato) ---
    items = []  # (kind, ...)
    for raw in src.splitlines():
        line = raw.split("#")[0].strip()
        # Etiquetas al inicio de linea (una o varias). No confundir con ':' dentro
        # de literales de cadena.
        m = LABEL_RE.match(line)
        while m:
            items.append(("label", m.group(1)))
            line = line[m.end():].strip()
            m = LABEL_RE.match(line)
        if not line:
            continue
        parts = line.replace(",", " ").split()
        mn = parts[0]
        ops = parts[1:]
        if mn in (".word", ".dword"):
            for w in ops:
                items.append(("word", w))
        elif mn == ".byte":
            for b in ops:
                items.append(("byte", b))
        elif mn in (".asciz", ".string", ".ascii"):
            s = line[line.index('"') + 1:line.rindex('"')]
            s = s.encode().decode("unicode_escape")
            for ch in s:
                items.append(("byte", str(ord(ch))))
            if mn != ".ascii":
                items.append(("byte", "0"))
        elif mn == ".space":
            for _ in range(int(ops[0], 0)):
                items.append(("byte", "0"))
        elif mn in (".text", ".data", ".globl", ".global", ".align", ".p2align"):
            continue  # ignorados
        else:
            for emn, eops in expand(mn, ops):
                items.append(("instr", emn, eops))

    # --- Pass 1: asignar direcciones y resolver etiquetas ---
    labels = {}
    addr = 0
    sized = []
    for it in items:
        if it[0] == "label":
            labels[it[1]] = addr
        elif it[0] == "word":
            sized.append((addr, it)); addr += 4
        elif it[0] == "byte":
            sized.append((addr, it)); addr += 1
        elif it[0] == "instr":
            sized.append((addr, it))
            if it[1] == "__li__":
                addr += 4 * li_reserved(it[2][1])
            else:
                addr += 4

    # --- Pass 2: emitir ---
    mem = bytearray()
    def emit_word(w):
        mem.extend(struct.pack("<I", w & 0xFFFFFFFF))
    def emit_byte(b):
        mem.append(b & 0xFF)

    for pc, it in sized:
        # alinear mem al address esperado (por datos de tamaño variable)
        while len(mem) < pc:
            mem.append(0)
        if it[0] == "word":
            emit_word(imm(it[1], labels))
        elif it[0] == "byte":
            emit_byte(imm(it[1], labels))
        elif it[0] == "instr":
            mn, ops = it[1], it[2]
            if mn == "__li__":
                rd = reg(ops[0])
                val = imm(ops[1], labels) & 0xFFFFFFFF
                words = encode_li(rd, val)
                for w in words:
                    emit_word(w)
                # rellenar con nop hasta la longitud reservada en pass1
                reserved = pc + 4 * li_reserved(ops[1])
                while len(mem) < reserved:
                    emit_word(0x13)  # nop = addi zero,zero,0
            else:
                emit_word(OPS[mn](ops, labels, pc))
    return bytes(mem)


def _is_int(tok):
    try:
        int(tok, 0)
        return True
    except ValueError:
        return False


def main():
    if len(sys.argv) != 3:
        print("uso: python asm.py entrada.s salida.bin")
        sys.exit(1)
    with open(sys.argv[1]) as f:
        data = assemble(f.read())
    with open(sys.argv[2], "wb") as f:
        f.write(data)
    print(f"{sys.argv[2]}: {len(data)} bytes")


if __name__ == "__main__":
    main()
