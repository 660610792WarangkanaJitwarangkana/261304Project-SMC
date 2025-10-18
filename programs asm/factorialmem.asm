        lw      0   1   N
        lw      0   6   FACTADDR
        jalr    6   7
        sw      0   3   OUT         ; save result in memory
        halt

; factorial(n)

factorial   beq     1   0   base_case
            sw      0   1   SAVEN
            lw      0   2   NEG1
            add     1   2   1
            lw      0   6   FACTADDR
            jalr    6   7
            lw      0   1   SAVEN
            add     0   0   4
mult_loop   beq     1   0   mult_done
            add     4   3   4
            lw      0   2   NEG1
            add     1   2   1
            beq     0   0   mult_loop
mult_done   add     0   4   3
            jalr    7   6

base_case   add     0   0   3
            add     3   3   3
            add     0   0   3          ; r3 = 1
            add     3   0   3
            jalr    7   6

; Data
N           .fill   3
NEG1        .fill   -1
SAVEN       .fill   0
OUT         .fill   0
FACT        .fill   factorial
