        lw   0   1   mcand   ; R1 = multiplicand (32766)
        lw   0   2   mplier  ; R2 = multiplier (10383)
        lw   0   3   zero    ; R3 = 0 (result)
        lw   0   4   one     ; R4 = 1
        lw   0   5   neg1    ; R5 = -1
        
loop    beq  2   0   done    ; if multiplier == 0, done
        add  3   1   3       ; result += multiplicand
        add  2   5   2       ; multiplier--
        beq  0   0   loop    ; repeat
        
done    halt

; Data
mcand   .fill 32766
mplier  .fill 10383
zero    .fill 0
one     .fill 1
neg1    .fill -1