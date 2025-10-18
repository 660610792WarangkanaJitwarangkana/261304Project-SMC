        lw      0   1   N          ; r1 = n (input)
        lw      0   6   FACT       ; r6 = address ของ factorial
        jalr    6   7              ; call factorial
        halt                       ; stop program

; factorial(n)
; input:  r1 = n
; output: r3 = factorial(n)

factorial   beq     1   0   base_case   ; if n == 0 → jump base case
            sw      0   1   SAVEN       ; save current n
            lw      0   2   NEG1        ; r2 = -1
            add     1   2   1           ; n = n - 1
            lw      0   6   FACT        ; r6 = factorial address
            jalr    6   7               ; recursive call factorial(n-1)
            lw      0   1   SAVEN       ; restore n
            add     0   0   4           ; r4 = 0 (accumulator)
mult_loop   beq     1   0   mult_done   ; if n == 0 → end multiply
            add     4   3   4           ; r4 = r4 + r3
            lw      0   2   NEG1        ; r2 = -1
            add     1   2   1           ; n = n - 1
            beq     0   0   mult_loop   ; loop again
mult_done   add     0   4   3           ; r3 = r4 (result)
            jalr    7   6               ; return

base_case   add     0   0   3           ; r3 = 0
            add     3   3   3           ; (dummy clear)
            add     0   0   3           ; r3 = 1 (base case)
            add     3   0   3
            jalr    7   6               ; return

; Data section
N           .fill   3           ; input n = 3
NEG1        .fill   -1
SAVEN       .fill   0
FACT        .fill   factorial
