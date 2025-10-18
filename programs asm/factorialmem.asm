        lw      0       1       N
        lw      0       6       FACTAD
        jalr    6       7       ; call FACTOR
        sw      0       3       OUT
        halt

; factorial(n) - recursive function
FACTOR  beq     1       0       BASE   ; if n == 0, go to base case
        
        ; Save registers
        sw      7       1       SAVER7 ; save return address
        sw      7       1       SAVEN  ; save n
        
        ; Prepare for recursive call: fact(n-1)
        lw      0       2       NEG1
        add     1       2       1      ; n = n-1
        lw      0       6       FACTAD
        jalr    6       7              ; recursive call: fact(n-1)
        
        ; Restore values
        lw      0       1       SAVEN  ; restore original n
        lw      0       7       SAVER7 ; restore return address
        
        ; Multiply n * fact(n-1)
        add     0       3       4      ; copy fact(n-1) to reg4
        add     0       0       3      ; clear reg3 for result
        beq     0       0       MULT

BASE    add     0       0       3      ; base case: fact(0) = 1
        add     3       3       3      ; reg3 = 2
        add     3       3       3      ; reg3 = 4  
        add     3       3       3      ; reg3 = 8
        lw      0       2       NEG1
        add     3       2       3      ; reg3 = 7
        jalr    7       6              ; return

; Multiplication: reg3 = reg1 * reg4
MULT    beq     1       0       MULTD  ; if counter == 0, done
        add     4       3       3      ; result += multiplier
        lw      0       2       NEG1
        add     1       2       1      ; counter--
        beq     0       0       MULT
MULTD   jalr    7       6              ; return

; Data section
N       .fill   3
NEG1    .fill   -1
SAVEN   .fill   0
SAVER7  .fill   0
OUT     .fill   0
FACTAD  .fill   FACTOR