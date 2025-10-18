        lw      0       1       N
        lw      0       6       FACTAD
        jalr    6       7
        sw      0       3       OUT
        halt

; factorial(n)
FACTOR  beq     1       0       BASE
        sw      0       1       SAVEN
        lw      0       2       NEG1
        add     1       2       1
        lw      0       6       FACTAD
        jalr    6       7
        lw      0       1       SAVEN
        add     0       0       4

MULT    beq     1       0       MULTD
        add     4       3       4
        lw      0       2       NEG1
        add     1       2       1
        beq     0       0       MULT
MULTD   add     0       4       3
        jalr    7       6

BASE    add     0       0       3
        add     3       3       3
        add     0       0       3
        add     3       0       3
        jalr    7       6

; Data
N       .fill   3
NEG1    .fill   -1
SAVEN   .fill   0
OUT     .fill   0
FACTAD  .fill   FACTOR
