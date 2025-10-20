        lw   0   1   n       ; R1 = n
        lw   0   2   r       ; R2 = r
        lw   0   5   stack   ; R5 = stack pointer
        lw   0   6   pos1    ; R6 = 1
        lw   0   7   neg1    ; R7 = -1
        
        ; Call combination(n, r)
        lw   0   4   comb    ; R4 = address of comb
        jalr 4   3           ; call comb, R3 = return address
        
        sw   0   3   result  ; store result
        halt

; combination function: R1 = n, R2 = r, returns R3 = result
comb    ; Base case: if r == 0, return 1
        beq  2   0   base
        
        ; Base case: if n == r, return 1
        beq  1   2   base
        
        ; Save registers to stack
        sw   5   3   0       ; push return address
        add  5   6   5       ; sp++
        sw   5   1   0       ; push n
        add  5   6   5       ; sp++
        sw   5   2   0       ; push r
        add  5   6   5       ; sp++
        
        ; First recursive call: combination(n-1, r)
        add  1   7   1       ; n = n - 1
        lw   0   4   comb    ; R4 = address of comb
        jalr 4   3           ; recursive call
        
        ; Save result of first call
        sw   5   3   0       ; push result1
        add  5   6   5       ; sp++
        
        ; Restore n and r for second call
        add  5   7   5       ; sp--
        lw   5   2   0       ; pop r
        add  5   7   5       ; sp--
        lw   5   1   0       ; pop n
        
        ; Second recursive call: combination(n-1, r-1)
        add  1   7   1       ; n = n - 1
        add  2   7   2       ; r = r - 1
        lw   0   4   comb    ; R4 = address of comb
        jalr 4   3           ; recursive call
        
        ; Get first result and add
        add  5   7   5       ; sp--
        lw   5   1   0       ; pop result1
        add  3   1   3       ; result = result1 + result2
        
        ; Restore return address and return
        add  5   7   5       ; sp--
        lw   5   3   0       ; pop return address
        jalr 3   0           ; return
        
base    lw   0   3   pos1    ; return 1
        jalr 3   0           ; return

; Data section
n       .fill 5
r       .fill 2
pos1    .fill 1
neg1    .fill -1
result  .fill 0
stack   .fill 100
comb    .fill 8              ; address of comb function.