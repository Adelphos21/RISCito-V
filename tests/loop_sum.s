# loop_sum.s — suma 1+2+...+10 = 55 con un bucle y lo imprime.
main:
    li   t0, 0           # acumulador
    li   t1, 1           # i = 1
    li   t2, 11          # limite (exclusivo)
loop:
    bge  t1, t2, done    # si i >= 11, terminar
    add  t0, t0, t1      # acc += i
    addi t1, t1, 1       # i++
    j    loop
done:
    mv   a0, t0          # a0 = 55
    li   a7, 1           # print_int
    ecall
    li   a0, 10          # '\n'
    li   a7, 11          # print_char
    ecall
    li   a7, 10          # exit
    ecall
