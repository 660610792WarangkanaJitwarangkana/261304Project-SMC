        lw      0   1   A        ; r1 = A
        lw      0   2   B        ; r2 = B
        add     0   0   3        ; r3 = 0 (result)
loop    beq     2   0   done     ; if (B == 0) goto done
        add     1   3   3        ; result = result + A
        add     2   7   2        ; B = B - 1  (r7 = -1)
        beq     0   0   loop     ; goto loop
done    sw      0   3   OUT      ; save result
        halt
A       .fill   3
B       .fill   4
OUT     .fill   0
neg1    .fill   -1