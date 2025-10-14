# test ตามรายละเอียด project ของอาจารย์
        lw 0 1 five     load reg1 with 5 (uses symbolic address)
        lw 1 2 3        load reg2 with -1 (uses numeric address)
start   add 1 2 1       decrement reg1
        beq 0 1 2       goto end of program when reg1==0
        beq 0 0 start   go back to the beginning of the loop
        noop
done    halt            end of program

five    .fill 5
neg1    .fill -1
stAddr  .fill start     will contain the address of start


#--------------------------------------------------------------
#เพิ่มเทสเข้ามา test.asm - fixed version
start1  add 1 2 3 
        lw 0 1 five
        beq 0 1 done1
        noop
done1   halt

# multiplication section
        lw 0 1 A        ; r1 = A
        lw 0 2 B        ; r2 = B
        add 0 0 3       ; r3 = 0 (result)
loop2   beq 2 0 done2   ; if (B == 0) goto done2
        add 1 3 3       ; result = result + A
        add 2 7 2       ; B = B - 1  (r7 = -1)
        beq 0 0 loop2
done2   sw 0 3 OUT
        halt

# decrement section
        lw 0 1 five     ; load reg1 with five
        lw 1 2 neg1     ; load reg2 with -1
loop3   add 1 2 1       # decrement reg1
        beq 0 1 done3   # goto done3 when reg1 == 0
        beq 0 0 loop3   # else loop again
        noop
done3 halt

# data area
A       .fill 3
B       .fill 4
OUT     .fill 0
neg2    .fill -1
five1   .fill 5
stAdd1  .fill loop2      # pointer to loop2 start

