        ; assume data section at bottom:
        ; labels: A, B, ONE, NEGONE, ZERO

        lw R1, A(R0)                    ; R1 = a (multiplicand)
        lw R2, B(R0)                    ; R2 = b (multiplier)
        lw R3, ZERO(R0)                 ; R3 = 0   (product)
        lw R5, ONE(R0)                  ; R5 = 1
        lw R4, NEGONE(R0)               ; R4 = -1  (for decrement)
Loop:   beq R2, R0, Done                ; if b == 0 goto Done
        add R3, R3, R1                  ; product = product + a
        add R2, R2, R4                  ; b = b + (-1)  -> decrement
        beq R0, R0, Loop                ; unconditional jump: if R0==R0 true -> branch to Loop (offset negative to go back)
Done:   sw R3, RESULT(R0)               ; store product to RESULT
        halt

; --- data (word addresses) ---
A:      .word  7       ; example a = 7
B:      .word  5       ; example b = 5
ONE:    .word  1
NEGONE: .word  -1
ZERO:   .word  0
RESULT: .word  0       ; space for result
