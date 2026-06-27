# symmetric.s — evalua si un arbol binario completo es simetrico.
# El arbol se asume ya colocado, aplanado por niveles, en 0x1000
# (hijo de i: 2i+1, 2i+2).  Cargalo antes de 'run' con, por ejemplo:
#     set 0x1000 6 4 4 1 2 2 1     ->  simetrico    ->  a0 = 1
#     set 0x1000 6 4 4 1 2 9 1     ->  NO simetrico ->  a0 = 0
#
#        6
#      /   \
#     4     4
#    / \   / \
#   1   2 2   1
#
# Invariantes globales:  s0 = base del arreglo,  s1 = n (numero de nodos)
main:
    li   s0, 0x1000      # base del arbol (precargado en memoria)
    li   s1, 7           # n = 7
    call is_symmetric    # a0 = 1 si es simetrico
    mv   s2, a0          # guardar resultado
    li   a7, 1           # print_int
    ecall
    li   a0, 10          # '\n'
    li   a7, 11          # print_char
    ecall
    mv   a0, s2          # dejar el resultado en a0 (esperado: 1)
    li   a7, 10          # exit
    ecall

# is_symmetric():  usa s0=base, s1=n.  Devuelve a0 = 0/1.
is_symmetric:
    li   t0, 2
    blt  s1, t0, is_true   # n < 2  ->  simetrico trivialmente
    li   a0, 1             # l = 1 (hijo izquierdo de la raiz)
    li   a1, 2             # r = 2 (hijo derecho de la raiz)
    j    mirror            # tail call: mirror retorna al llamador
is_true:
    li   a0, 1
    ret

# mirror(a0=l, a1=r):  ¿son espejo el subarbol l y el subarbol r?
# Devuelve a0 = 0/1.  Usa s0=base, s1=n.
mirror:
    bge  a0, s1, m_loob    # l fuera de rango
    bge  a1, s1, m_ret0    # l dentro, r fuera  ->  0
    slli t0, a0, 2
    add  t0, s0, t0
    lw   t1, 0(t0)         # arr[l]
    slli t2, a1, 2
    add  t2, s0, t2
    lw   t3, 0(t2)         # arr[r]
    bne  t1, t3, m_ret0    # arr[l] != arr[r]  ->  0
    addi sp, sp, -16
    sw   ra, 12(sp)
    sw   a0, 8(sp)         # guardar l
    sw   a1, 4(sp)         # guardar r
    # 1er llamado: mirror(2l+1, 2r+2)
    slli t0, a0, 1
    addi t0, t0, 1
    slli t1, a1, 1
    addi t1, t1, 2
    mv   a0, t0
    mv   a1, t1
    call mirror
    beqz a0, m_pop0        # si el 1er subarbol no es espejo  ->  0
    # 2do llamado: mirror(2l+2, 2r+1)
    lw   a0, 8(sp)
    lw   a1, 4(sp)
    slli t0, a0, 1
    addi t0, t0, 2
    slli t1, a1, 1
    addi t1, t1, 1
    mv   a0, t0
    mv   a1, t1
    call mirror
    j    m_pop             # a0 = resultado del 2do subarbol
m_pop0:
    li   a0, 0
m_pop:
    lw   ra, 12(sp)
    addi sp, sp, 16
    ret
m_loob:
    bge  a1, s1, m_ret1    # ambos fuera de rango  ->  espejo (1)
    li   a0, 0             # solo l fuera  ->  0
    ret
m_ret0:
    li   a0, 0
    ret
m_ret1:
    li   a0, 1
    ret
