        lw      0       1       N
        lw      0       6       FACTAD
        jalr    6       7
        halt

N       .fill   3
ONE     .fill   1
NEG1    .fill   -1
SAVEN   .fill   0
SAVER7  .fill   0
FACTAD  .fill   FACTOR

FACTOR  beq     1       0       BASE
        sw      0       1       SAVEN
        sw      0       7       SAVER7
        lw      0       2       NEG1
        add     1       2       1
        lw      0       6       FACTAD
        jalr    6       7
        lw      0       1       SAVEN
        lw      0       7       SAVER7
        add     0       3       4
        add     0       0       3
        add     0       1       2
MULT    beq     2       0       MULTD
        add     3       4       3
        lw      0       5       NEG1
        add     2       5       2
        beq     0       0       MULT
MULTD   jalr    7       6
BASE    lw      0       3       ONE
        jalr    7       6