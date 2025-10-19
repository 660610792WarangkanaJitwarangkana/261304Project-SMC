        ; --- main ---

        lw R1, N(R0)                    ; load n
        lw R7, SP_INIT(R0)              ; initialize stack pointer
        jalr FACT_ADDR(R0), R5          ; call factorial: store return addr in R5, jump to FACT_ADDR
                                        ; after return, result in R6 (ตาม convention)
        
        sw R6, FACT_RESULT(R0)
        halt

; --- factorial function at label FACT_ADDR ---
; expects input n in R1
; returns result in R6

FACT_ADDR:
        beq R1, R0, FACT_BASE           ; if n == 0 return 1
                                        
        ; -- recursive case --
        ; save caller-saved registers and return-address R5 onto stack

        add R7, R7, R4                  ; R4 holds NEG1 (decrement SP) -> SP = SP - 1
        sw R5, 0(R7)                    ; store return address
        add R7, R7, R4
        sw R1, 0(R7)                    ; store n (current)
        
        ; compute n-1
        add R1, R1, NEGREG              ; R1 = R1 - 1
        
        ; call FACT_ADDR recursively
        jalr FADDR, R5                  ; need function address in FACT_ADDR_REG, R5 will get return addr
        
        ; after return: result in R6
        ; restore n
        lw R1, 0(R7)
        add R7, R7, R5_CONST            ; add SP +1 (pop) ; (R5_CONST==1)
        lw R5, 0(R7)                    ; restore return address
        add R7, R7, R5_CONST            ; adjust SP back (pop)

        ; multiply: result = n * R6 (where R6 currently holds factorial(n-1))
        ; original n is now in R1
        ; we need to compute R6 = R1 * R6 -> call multiplication routine or inline loop
        ; For brevity, do repeated-add multiplication inline using R2,R3,R4,R5 as temps

        lw R5, ONE(R0)                  ; load one
        lw R4, NEGONE(R0)               ; load -1
        lw R2, ZERO(R0)
        add R3, R3, R3                  ; (ensure R3=0)  -- but better set R3=0 by lw
        lw R3, ZERO(R0)                 ; R3 = 0  (accumulator)

MulLoop:
        beq R6, R0, MulDone             ; if multiplier (R6) == 0 done
        add R3, R3, R1                  ; R3 += n
        add R6, R6, R4                  ; R6 -= 1
        beq R0, R0, MulLoop

MulDone:
        add R6, R3, R0                  ; move product to R6 (return reg)
        jalr R5, R0                     ; return using saved return address in R5  (but careful with R0)
        ; note: better to do `jalr R5, R4` to jump to R5 and overwrite R4

FACT_BASE:
        lw R6, ONE(R0)                  ; return 1
        jalr R5, R4                     ; return (jump back to address in R5)
