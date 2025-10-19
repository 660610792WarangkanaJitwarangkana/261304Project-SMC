; Factorial Program for SMC Architecture
; Implements recursive factorial function

        lw      0       1       N       ; Load n into r1
        lw      0       6       FACTAD  ; Load factorial function address
        jalr    6       7               ; Call factorial function
        sw      0       3       OUT     ; Store result
        halt                            ; Stop program

; Factorial function: r3 = factorial(r1)
FACTOR  beq     1       0       BASE    ; Base case: if n == 0
        
        ; Save state for recursion
        sw      0       1       SAVEN   ; Save current n
        sw      0       7       SAVER7  ; Save return address
        
        ; Prepare recursive call: factorial(n-1)
        lw      0       2       NEG1    ; r2 = -1
        add     1       2       1       ; r1 = n - 1
        lw      0       6       FACTAD  ; Load function address
        jalr    6       7               ; Recursive call
        
        ; Restore state after recursion
        lw      0       1       SAVEN   ; Restore original n
        lw      0       7       SAVER7  ; Restore return address
        
        ; Multiply n * factorial(n-1)
        ; r3 currently has factorial(n-1), r1 has n
        add     0       3       4       ; r4 = factorial(n-1)
        add     0       0       3       ; r3 = 0 (initialize result)
        add     0       1       2       ; r2 = n (counter)
        
MULT    beq     2       0       MULTD   ; If counter == 0, done
        add     3       4       3       ; r3 = r3 + r4
        lw      0       5       NEG1    ; r5 = -1
        add     2       5       2       ; counter--
        beq     0       0       MULT    ; Continue multiplication loop
        
MULTD   jalr    7       6               ; Return from factorial

; Base case: factorial(0) = 1
BASE    lw      0       3       ONE     ; r3 = 1
        jalr    7       6               ; Return

; Data section
N       .fill   3       ; Input value (change for different test cases)
ONE     .fill   1       ; Constant 1
NEG1    .fill   -1      ; Constant -1
SAVEN   .fill   0       ; Storage for n during recursion
SAVER7  .fill   0       ; Storage for return address
OUT     .fill   0       ; Output storage
FACTAD  .fill   FACTOR  ; Address of factorial function