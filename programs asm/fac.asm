# factorial_recursive_memory.asm
# fact(n): return n! stored in R1

# Register ใช้ดังนี้:
# R0 = 0
# R1 = return value
# R2 = n
# R3 = temp
# R4 = temp
# R6 = stack pointer (SP)
# R7 = return address

    ADDI R2, R0, 5      # n = 5
    ADDI R6, R0, 100     # SP = 100 (top of memory stack)
    JAL R0, fact         # เรียก fact(n)
    HALT

# -------------------
fact:
    # base case: if n <= 1 → return 1
    BLE R2, R0, base_case
    ADDI R3, R0, 1
    BEQ R2, R3, base_case

    # push return address (address after call)
    ADDI R7, R0, after_call
    SW R7, 0(R6)
    ADDI R6, R6, -1

    # push current n
    SW R2, 0(R6)
    ADDI R6, R6, -1

    # recursive call: fact(n-1)
    ADDI R2, R2, -1
    JAL R0, fact

after_call:
    # pop n
    ADDI R6, R6, 1
    LW R4, 0(R6)

    # compute result = n * result
    MUL R1, R1, R4

    # pop return address
    ADDI R6, R6, 1
    LW R7, 0(R6)

    JAL R0, R7           # return to caller

base_case:
    ADDI R1, R0, 1       # result = 1
    JAL R0, R7           # return
