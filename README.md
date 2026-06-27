# Simulador RISC-V (RV32I)

Simulador del ISA base RISC-V de 32 bits (**RV32I**), implementado en C++17.
Permite cargar un binario crudo, ejecutarlo paso por paso o de corrido, e
inspeccionar el PC, los 32 registros y la memoria desde un REPL de terminal.

> CS3051 — Arquitectura de Computadores · HW#6 / Tarea 4

---

## Características

- **ISA RV32I completo**: las 37 instrucciones del Anexo A (aritmética, lógica,
  shifts, loads/stores, branches, `jal`/`jalr`, `lui`/`auipc`).
- **Memoria plana little-endian** de 1 MiB, byte-addressable, con verificación de
  rango.
- **Carga de binarios crudos** en la dirección `0x00000000`.
- **Ejecución paso por paso** (`step`) y continua (`run`).
- **Inspección** de PC, registros (por nombre `x5` o ABI `sp`, `a0`, …) y memoria.
- **`ecall`** estilo SPIM (imprimir entero/cadena/carácter, leer entero/cadena,
  salir) — ver tabla abajo.
- **Desensamblador** integrado: `step` y `disasm` muestran la instrucción.
- **`ebreak`** y **detección de bucle infinito** (`done: beq x0,x0,done`) para que
  `run` termine de forma natural.
- **Mini-ensamblador** en Python (`tests/asm.py`) para generar binarios de prueba
  reproducibles (no es parte del simulador; sólo una utilidad de pruebas).

---

## Compilación

Requiere un compilador C++17 (probado con **g++ 15.1 / MSYS2 ucrt64**).

### Opción A — script directo (recomendado)

```bash
./build.sh           # Linux / Git Bash / MSYS2
build.bat            # Windows (cmd)
```

Genera el ejecutable `riscv-sim` (o `riscv-sim.exe`).

### Opción B — CMake

```bash
cmake -S . -B build
cmake --build build
```

> **Nota (Windows/MSYS2):** asegúrate de que `C:\msys64\ucrt64\bin` esté en el
> `PATH` para que `g++` encuentre sus subprocesos (`as`, `collect2`).
>
> El build usa `-O1`: en este toolchain, `-O2` provocaba fallos esporádicos al
> *cargar* el ejecutable (ajenos al código del simulador); `-O1` es estable y
> sigue siendo código optimizado. La velocidad no es relevante para un simulador.

---

## Uso

```text
$ ./riscv-sim tests/riscvtest.bin

"tests/riscvtest.bin" cargado a memoria.
Escriba 'help' para ver los comandos.

> step
0x00000000:  00500113   addi sp, zero, 5

> regs x2
x2  (sp  ) = 0x00000005  (5)

> run
Ejecutadas 16 instrucciones. Estado: bucle infinito (programa terminado).

> mem 0x64 0x67
Memoria (0x00000064-0x00000067):
  0x00000064: 19 00 00 00

> exit

See you next time...
```

### Comandos del REPL

| Comando | Descripción |
|---------|-------------|
| `step [n]` | Ejecuta `n` instrucciones (def. 1), mostrando cada una desensamblada. |
| `run [max]` | Ejecuta hasta detenerse (límite opcional, def. 10 000 000). |
| `pc` | Muestra el contador de programa. |
| `regs [r ...]` | Muestra registros (todos, o los indicados: `x5`, `sp`, `a0`, …). |
| `setreg <r> <val>` | Asigna un valor a un registro. |
| `mem <ini> [fin]` | Vuelca memoria en hexadecimal en el rango `[ini, fin]`. |
| `set <addr> <w ...>` | Escribe words de 32 bits (útil para precargar datos). |
| `setb <addr> <b ...>` | Escribe bytes en memoria. |
| `disasm <ini> [fin]` | Desensambla instrucciones en el rango. |
| `load <archivo>` | Carga un binario crudo en `0x0`. |
| `reset` | Reinicia PC y registros (conserva la memoria). |
| `help` | Ayuda. |
| `exit` \| `quit` | Sale del simulador. |

Los números aceptan prefijo `0x` (hex) o decimal.

### Environment calls (`ecall`)

El número de servicio se pasa en `a7` (registro `x17`):

| `a7` | Servicio | Argumentos / Resultado |
|------|----------|------------------------|
| 1 | `print_int` | imprime `a0` (con signo) |
| 4 | `print_string` | imprime la cadena ASCIIZ en `a0` |
| 5 | `read_int` | lee un entero → `a0` |
| 8 | `read_string` | lee una línea en el buffer `a0` (long. máx. `a1`) |
| 10 | `exit` | termina la ejecución |
| 11 | `print_char` | imprime el carácter `a0` |
| 17 | `exit2` | termina (código en `a0`) |

---

## Programas de prueba

En `tests/` hay 4 programas con su fuente `.s` y su binario `.bin` ya ensamblado:

| Programa | Qué prueba | Resultado esperado |
|----------|-----------|--------------------|
| `riscvtest` | add, sub, and, or, slt, addi, lw, sw, beq, jal | `mem[0x64] = 25 (0x19)` |
| `loop_sum` | bucle con branches, `ecall` print_int | imprime `55` |
| `ecall_demo` | `ecall` print_string / print_int / print_char | `Hola RISC-V! La respuesta es: 42` |
| `quicksort` | recursión (`jal`/`jalr`, stack), loads/stores | `{6,4,3,2,1,8,9}` → `{1,2,3,4,6,8,9}` en `0x1000` |
| `symmetric` | recursión, evalúa si un árbol es simétrico | `a0 = 1` (árbol `{6,4,4,1,2,2,1}` precargado en `0x1000`) |

Ejemplo de verificación de `riscvtest`:

```bash
printf 'run\nmem 0x64 0x67\nexit\n' | ./riscv-sim tests/riscvtest.bin
# -> Memoria (0x00000064-0x00000067):  19 00 00 00
```

Ejemplo de `quicksort` (el programa inicializa el arreglo en `0x1000`):

```bash
printf 'run\nmem 0x1000 0x101b\nexit\n' | ./riscv-sim tests/quicksort.bin
# -> 01 00 00 00 02 00 00 00 03 00 00 00 04 00 00 00 06 ... 08 ... 09 ...
```

### Regenerar los binarios de prueba

```bash
cd tests
python asm.py riscvtest.s riscvtest.bin
python asm.py loop_sum.s  loop_sum.bin
python asm.py ecall_demo.s ecall_demo.bin
python asm.py quicksort.s quicksort.bin
python asm.py symmetric.s symmetric.bin
```

Para el árbol simétrico, el árbol se precarga en memoria antes de ejecutar:

```bash
printf 'set 0x1000 6 4 4 1 2 2 1\nrun\nregs a0\nexit\n' | ./riscv-sim tests/symmetric.bin
# -> a0 = 1  (simétrico).  Con {6 4 4 1 2 9 1} -> a0 = 0
```

> También puedes cargar binarios crudos descargados de
> [CPUlator](https://cpulator.01xz.net/?sys=rv32) (formato *Raw*, ver Anexo B del
> enunciado). Para los programas que esperan datos en `0x1000`, precárgalos con
> `set 0x1000 6 4 3 2 1 8 9` antes de `run`.

---

## Estructura del proyecto

```
.
├── src/
│   ├── simulator.hpp / .cpp   # Estado de la CPU, decode y ejecución (RV32I, ecall)
│   ├── disasm.hpp / .cpp      # Desensamblador
│   └── main.cpp               # REPL de terminal
├── tests/
│   ├── asm.py                 # Mini-ensamblador RV32I -> binario crudo
│   └── *.s / *.bin            # Programas de prueba
├── CMakeLists.txt
├── build.sh / build.bat
└── README.md
```

---

## Detalles de implementación

- El ciclo de ejecución es **fetch → decode → execute → writeback**. Cada `step`
  lee la instrucción de 32 bits apuntada por el PC, la decodifica según su formato
  (R/I/S/B/U/J), ejecuta su comportamiento y actualiza el estado.
- El registro `x0` está **cableado a cero**: cualquier escritura se descarta.
- El stack pointer (`x2`) se inicializa al **tope de la memoria** (`0x100000`).
- Los accesos a memoria fuera del rango `[0, 1 MiB)` se reportan como error de
  ejecución y detienen la CPU, en vez de provocar comportamiento indefinido.
- No se modelan permisos ni segmentación (memoria plana), tal como permite el
  enunciado.
