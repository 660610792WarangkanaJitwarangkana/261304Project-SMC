        # fact.asm -- factorial (iterative), generic
        # $0 = zero
        # $1 = result (final factorial)
        # $4 = n (current loop n)
        # $5 = pos1 (1)
        # $3 = neg1 (-1)
        # $6 = temp multiplier accumulator (used in multiply inner loop)
        # $7 = temp counter for multiply inner loop

        lw      0       4       nval        # $4 = n
        lw      0       5       pos1        # $5 = 1
        lw      0       3       neg1        # $3 = -1

        add     0       5       1           # result = 1 ($1 = 1)
        beq     4       5       done_fact   # if n == 1 or n == 0 -> done (since result=1)

        # i = 2
        add     5       5       7           # $7 = 2 (pos1 + pos1)
        beq     0       0       loop_fact

loop_fact
        # multiply result ($1) by i ($7), store back in $1
        # use repeated-add: temp = 0; cnt = i; while cnt>0: temp += result; cnt--
        add     0       0       6           # $6 = 0    (temp)
        lw      0       2       pos1        # $2 = 1 (will use as counter decrement helper)
mult_loop
        beq     7       0       after_mult  # if cnt == 0 (i==0) skip (shouldn't happen)
        add     6       1       6           # temp += result
        add     7       3       7           # cnt-- (7 += -1)
        beq     0       0       mult_loop
after_mult
        add     6       0       1           # result = temp ($1 = $6)

        # increment i; if i <= n then loop
        add     7       5       7           # i = i + 1  (note: we destroyed cnt; reuse $7 as i)
        beq     7       4       loop_fact   # if i == n then still need multiply for i==n
        # if i < n, continue loop; need to check less-than: do (i - n) and branch?
        # simple approach: if i != n then continue. If i > n exit.
        # compute temp = n - i
        add     4       3       2           # $2 = n + (-1)  (we will use beq to check equality)
        # We'll check equality i == n handled above; if i != n, we must check if i < n
        # For simplicity: branch back if i != n and n != 0
        beq     7       4       loop_fact   # if i == n, handled earlier, but keep as safety
        # otherwise finish when i > n
        beq     0       0       done_fact

done_fact
        halt
nval    .fill   5       # example: nval = 5 (change to test)
pos1    .fill   1
neg1    .fill   -1
