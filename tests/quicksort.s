# quicksort.s — quicksort recursivo in-place (particion de Lomuto).
# Coloca {6,4,3,2,1,8,9} en 0x1000 y lo ordena -> {1,2,3,4,6,8,9}.
main:
    li   s0, 0x1000      # base del arreglo
    li   t0, 6
    sw   t0, 0(s0)
    li   t0, 4
    sw   t0, 4(s0)
    li   t0, 3
    sw   t0, 8(s0)
    li   t0, 2
    sw   t0, 12(s0)
    li   t0, 1
    sw   t0, 16(s0)
    li   t0, 8
    sw   t0, 20(s0)
    li   t0, 9
    sw   t0, 24(s0)
    # quicksort(base, lo=0, hi=6)
    mv   a0, s0
    li   a1, 0
    li   a2, 6
    call qsort
    li   a7, 10          # exit
    ecall

# qsort(a0=base, a1=lo, a2=hi)  — indices con signo
qsort:
    bge  a1, a2, qs_ret  # if lo >= hi: return
    addi sp, sp, -20
    sw   ra, 16(sp)
    sw   s0, 12(sp)
    sw   s1, 8(sp)
    sw   s2, 4(sp)
    sw   s3, 0(sp)
    mv   s0, a0          # base
    mv   s1, a1          # lo
    mv   s2, a2          # hi
    # pivot = arr[hi]
    slli t0, s2, 2
    add  t0, s0, t0
    lw   t1, 0(t0)       # t1 = pivot
    addi t2, s1, -1      # i = lo - 1
    mv   t3, s1          # j = lo
part_loop:
    bge  t3, s2, part_done
    slli t4, t3, 2
    add  t4, s0, t4      # &arr[j]
    lw   t5, 0(t4)       # arr[j]
    bge  t5, t1, part_skip   # if arr[j] >= pivot: skip
    addi t2, t2, 1       # i++
    slli t6, t2, 2
    add  t6, s0, t6      # &arr[i]
    lw   a3, 0(t6)       # tmp = arr[i]
    lw   a4, 0(t4)       # arr[j]
    sw   a4, 0(t6)       # arr[i] = arr[j]
    sw   a3, 0(t4)       # arr[j] = tmp
part_skip:
    addi t3, t3, 1
    j    part_loop
part_done:
    addi s3, t2, 1       # p = i + 1
    slli t0, s3, 2
    add  t0, s0, t0      # &arr[p]
    slli t4, s2, 2
    add  t4, s0, t4      # &arr[hi]
    lw   a3, 0(t0)
    lw   a4, 0(t4)
    sw   a4, 0(t0)       # swap arr[p], arr[hi]
    sw   a3, 0(t4)
    # qsort(base, lo, p-1)
    mv   a0, s0
    mv   a1, s1
    addi a2, s3, -1
    call qsort
    # qsort(base, p+1, hi)
    mv   a0, s0
    addi a1, s3, 1
    mv   a2, s2
    call qsort
    lw   ra, 16(sp)
    lw   s0, 12(sp)
    lw   s1, 8(sp)
    lw   s2, 4(sp)
    lw   s3, 0(sp)
    addi sp, sp, 20
qs_ret:
    ret
