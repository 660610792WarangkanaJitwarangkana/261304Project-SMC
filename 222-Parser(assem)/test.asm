# test.asm - example input for parser unit test

        lw 0 1 five       # load reg1 with five
        lw 1 2 neg1       ; load reg2 with -1

loop    add 1 2 1         # decrement reg1
        beq 0 1 done      # goto done when reg1 == 0
        beq 0 0 loop      # else loop again
        noop

done    halt

# data area
five    .fill 5
neg1    .fill -1
stAddr  .fill loop        # pointer to loop start

# additional test instructions
start   add 1 2 3
        lw 0 1 five
        beq 0 1 done2
        noop

done2   halt
five2   .fill 5

# intentionally blank line above to test countBlankLines=true option
