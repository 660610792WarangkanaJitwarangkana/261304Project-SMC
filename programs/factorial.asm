;$0 = zero
;$1 = result (final factorial)
;$2 = counter for multiply
;$3 = neg1 (-1)  
;$4 = n (input)
;$5 = pos1 (1)
;$6 = temp accumulator
;$7 = i (loop counter)

        lw      0       4       nval        # $4 = n
        lw      0       5       pos1        # $5 = 1
        lw      0       3       neg1        # $3 = -1

        add     0       5       1           # result = 1
        add     5       5       7           # i = 2
        
loopf   add     0       0       6           # temp = 0
        add     0       7       2           # counter = i
multlo  beq     2       0       aftmul      # multiply loop
        add     6       1       6           # temp += result
        add     2       3       2           # counter--
        beq     0       0       multlo
aftmul  add     6       0       1           # result = temp

        add     7       5       7           # i++
        
        ; ตรวจสอบ i > n โดยใช้ n+1
        add     4       5       6           # $6 = n + 1 (ใช้ $6 ชั่วคราว)
        beq     7       6       donefa      # if i == n+1 → done
        beq     0       0       loopf
        
donefa  halt

nval    .fill   5
pos1    .fill   1
neg1    .fill   -1