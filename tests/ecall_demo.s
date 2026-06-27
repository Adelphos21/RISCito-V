# ecall_demo.s — demuestra las environment calls (estilo SPIM).
# Imprime un texto seguido de un entero y un salto de linea, luego termina.
main:
    la   a0, msg         # direccion del texto
    li   a7, 4           # servicio 4 = print_string
    ecall
    li   a0, 42          # entero a imprimir
    li   a7, 1           # servicio 1 = print_int
    ecall
    li   a0, 10          # '\n'
    li   a7, 11          # servicio 11 = print_char
    ecall
    li   a7, 10          # servicio 10 = exit
    ecall
msg:
    .asciz "Hola RISC-V! La respuesta es: "
