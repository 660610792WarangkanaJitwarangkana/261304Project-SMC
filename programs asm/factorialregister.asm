        lw      0       1       N       ; r1 = n (input)
        lw      0       6       FACTAD  ; r6 = address ของ factorial
        jalr    6       7       ; call factorial
        halt                    ; stop program

; factorial(n)
; input:  r1 = n
; output: r3 = factorial(n)

FACTOR  beq     1       0       BASE    ; if n == 0 → jump base case
        
        ; Save state
        sw      0       1       SAVEN   ; save current n
        sw      0       7       SAVER7  ; save return address
        
        ; Prepare recursive call
        lw      0       2       NEG1    ; r2 = -1
        add     1       2       1       ; n = n - 1
        lw      0       6       FACTAD  ; r6 = factorial address
        jalr    6       7       ; recursive call factorial(n-1)
        
        ; Restore state
        lw      0       1       SAVEN   ; restore n
        lw      0       7       SAVER7  ; restore return address
        
        ; Multiply n * fact(n-1)
        add     0       3       4       ; r4 = fact(n-1)
        add     0       0       3       ; r3 = 0 (result)
        add     0       1       2       ; r2 = n (counter)

MULT    beq     2       0       MULTD   ; if counter == 0 → end multiply
        add     3       4       3       ; r3 = r3 + r4
        lw      0       5       NEG1    ; r5 = -1
        add     2       5       2       ; counter = counter - 1
        beq     0       0       MULT    ; loop again

MULTD   jalr    7       6       ; return

BASE    lw      0       3       ONE     ; r3 = 1 (base case)
        jalr    7       6       ; return

; Data section
N       .fill   3       ; input n = 3
NEG1    .fill   -1
ONE     .fill   1
SAVEN   .fill   0
SAVER7  .fill   0
FACTAD  .fill   FACTOR