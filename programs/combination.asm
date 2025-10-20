        lw      0       1       nval        ; reg1 = n
        lw      0       2       rval        ; reg2 = r
        lw      0       3       one         ; reg3 = 1
        lw      0       4       zero        ; reg4 = 0
        lw      0       5       neg1        ; reg5 = -1
        lw      0       6       result      ; reg6 = 1 (result)
        lw      0       7       tmp         ; reg7 = temp counter

        beq     2       0       base        ; if r == 0 return 1
        beq     1       2       base        ; if n == r return 1

        lw      0       7       zero        ; i = 0

loop    beq     7       2       done        ; if i == r stop
        add     1       5       1           ; n = n - 1
        add     6       1       6           ; result += n
        add     7       3       7           ; i++
        beq     0       0       loop        ; repeat

done    halt

base    halt

; ===== Data =====
nval    .fill   5
rval    .fill   3
one     .fill   1
zero    .fill   0
neg1    .fill   -1
result  .fill   1
tmp     .fill   0
