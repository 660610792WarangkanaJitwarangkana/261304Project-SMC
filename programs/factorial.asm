<<<<<<< HEAD
;$0 = zero
;$1 = result (final factorial)
;$4 = n (input)
;$5 = pos1 (1)
;$3 = neg1 (-1)
;$6 = temp accumulator (used in multiply inner loop)
;$7 = temp counter for multiply inner loop
=======
# fact.asm -- factorial (iterative)
# $0 = zero
# $1 = result (final factorial)
# $4 = n (input)
# $5 = pos1 (1)
# $3 = neg1 (-1)
# $6 = temp accumulator (used in multiply inner loop)
# $7 = temp counter for multiply inner loop

>>>>>>> 00a4ad3d3fb5b880243a0fb75920d26eba4ff61b
        lw      0       4       nval        # $4 = n
        lw      0       5       pos1        # $5 = 1
        lw      0       3       neg1        # $3 = -1

        add     0       5       1           # result = 1 ($1 = 1)
<<<<<<< HEAD
        beq     4       5       donefa      # if n == 1 → done

        add     5       5       7           # $7 = 2 (i = 2)
        beq     0       0       loopf       # jump to factorial loop

loopf   add     0       0       6           # $6 = 0 (temp = 0)
        add     0       7       2           # $2 = i (copy counter)
multlo  beq     2       0       aftmul      # if counter == 0 → end multiply
        add     6       1       6           # temp += result
        add     2       3       2           # counter-- ($2 = $2 - 1)
        beq     0       0       multlo      # loop multiply
aftmul  add     6       0       1           # result = temp

        add     7       5       7           # i = i + 1
        add     4       5       8           # $8 = n + 1
        beq     7       8       donefa      # if i == n+1 → done  ← แก้ไขตรงนี้!
        beq     0       0       loopf       # else continue factorial

donefa  halt                                 # stop program

;Data
nval    .fill   5                           # input n = 5
=======
        beq     4       5       donefa   # if n == 1 → done

        add     5       5       7           # $7 = 2 (i = 2)
        beq     0       0       loopf     # jump to factorial loop

loopf  add     0       0       6           # $6 = 0 (temp = 0)
        add     0       7       2           # $2 = i (copy counter)
multlo  beq     2       0       aftmul       # if counter == 0 → end multiply
        add     6       1       6           # temp += result
        add     2       3       2           # counter-- ($2 = $2 - 1)
        beq     0       0       multlo       # loop multiply
aftmul  add     6       0       1           # result = temp

        add     7       5       7           # i = i + 1
        beq     7       4       donefa   # if i == n → done
        beq     0       0       loopf      # else continue factorial

donefa halt                           # stop program

# Data
nval    .fill   5       # input n = 5 (change as desired)
>>>>>>> 00a4ad3d3fb5b880243a0fb75920d26eba4ff61b
pos1    .fill   1
neg1    .fill   -1