        # mult.asm  -- multiply mcand * mplier (generic)
        # registers:
        # $0 = zero
        # $1 = result (final product)
        # $4 = mcand (loaded)
        # $6 = mplier (loaded, will be decremented)
        # $5 = pos1 (constant 1)
        # $3 = neg1 (constant -1)

        lw      0       4       mcand       # $4 = memory[mcand]
        lw      0       6       mplier      # $6 = memory[mplier]
        lw      0       5       pos1        # $5 = 1
        lw      0       3       neg1        # $3 = -1
        noop
loop    beq     6       0       done        # if mplier == 0 -> done
        add     1       4       1           # result += mcand
        add     6       3       6           # mplier += (-1)  ; decrement
        beq     0       0       loop
done    halt
mcand   .fill   0                       # placeholder: put multiplicand here
mplier  .fill   0                       # placeholder: put multiplier here
pos1    .fill   1
neg1    .fill   -1
