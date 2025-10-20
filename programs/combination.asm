        lw      0       4       stackBase   ; initialize SP
        lw      0       5       combAddr    ; load function address
        lw      0       6       nVal        ; n = 5
        lw      0       7       rVal        ; r = 3
        jalr    5       5                   ; call comb(n, r)
        halt

; function comb(n, r)
comb    add     4       4       -1          ; push space for RA
        sw      4       5       0
        add     4       4       -1          ; push n
        sw      4       6       0
        add     4       4       -1          ; push r
        sw      4       7       0

        ; if (r == 0) return 1
        beq     7       0       baseCase

        ; if (n == r) return 1
        add     6       -7      1           ; temp = n - r
        beq     1       0       baseCase

        ; recursive call 1: comb(n-1, r)
        add     6       -1      6           ; n = n - 1
        jalr    5       5
        lw      4       1       0           ; save result1

        ; recursive call 2: comb(n-1, r-1)
        add     6       -1      6
        add     7       -1      7
        jalr    5       5
        lw      4       2       0           ; save result2

        ; add result1 + result2
        add     1       2       3
        beq     0       0       returnComb

baseCase
        lw      0       3       one         ; result = 1

returnComb
        ; pop stack
        lw      4       7       0
        add     4       4       1
        lw      4       6       0
        add     4       4       1
        lw      4       5       0
        add     4       4       1
        jalr    5       0                   ; return

; Data section
stackBase .fill   1024
combAddr  .fill   comb
nVal      .fill   5
rVal      .fill   3
one       .fill   1
neg1      .fill   -1
