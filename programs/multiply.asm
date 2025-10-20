        lw      0       1       num1        load reg1 = 32766
        lw      0       2       num2        load reg2 = 10383
        lw      0       3       zero        load reg3 = 0
        lw      0       4       neg1        load reg4 = -1

loop    beq     2       0       done        if reg2 == 0 → done
        add     3       1       3           result += num1
        add     2       4       2           num2 -= 1
        beq     0       0       loop        jump back to loop

done    halt
num1    .fill   32766
num2    .fill   10383
zero    .fill   0
neg1    .fill   -1
