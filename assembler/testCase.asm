# Case 1 : Valid complete program
# - ทดสอบ assembler ทำงานครบทุกคำสั่งแบบถูกต้อง
# - ไม่มี error

        lw      0   1   five        load reg1 with 5
        lw      1   2   neg1        load reg2 with -1
start   add     1   2   1           decrement reg1
        beq     0   1   done        goto end when reg1==0
        beq     0   0   start       go back to start
        noop
done    halt

five    .fill 5
neg1    .fill -1
stAddr  .fill start


# Case 2 : All instruction types
# - ครบทุก opcode (R/I/J/O) และ .fill
# - ไม่มี error

# R-type
        add     0   1   2
        nand    2   3   4

# I-type
        lw      0   1   five
        sw      1   2   five
        beq     0   1   end

# J-type
        jalr    1   2

# O-type
        halt
        noop

# .fill
five    .fill 123
end     .fill five


# Case 3 : BEQ backward branch
# - ทดสอบ offset ติดลบ (branch ย้อนกลับ)
# - ไม่มี error

start add 0 0 1
beq 0 1 start
halt


# Case 4 : Undefined label
# - ใช้ label "notExist" ที่ไม่มีในโปรแกรม
# - error

lw 0 1 five
add 1 1 2
beq 1 2 notExist   # undefined label
halt
five .fill 5


# Case 5 : Duplicate label
# - label "start" ถูกประกาศซ้ำ
# - error

start lw 0 1 five
start add 1 1 2
halt
five .fill 5


# Case 6 : Register out of range
# - regA = 8 ผิด (ค่าต้องอยู่ในช่วง 0–7)
# - error

add 8 0 1
halt


# Case 7 : Offset out of 16-bit range
# - offset 40000 เกินขอบเขต signed 16-bit (-32768..32767)
# - error

lw 0 1 40000
halt


# Case 8 : Invalid opcode
# - ไม่มี opcode ชื่อ "lww"
# - error

lww 0 1 5
halt


# Case 9 : .fill 
# - ไม่มีค่าตามหลัง .fill
# - error

.fill
halt
