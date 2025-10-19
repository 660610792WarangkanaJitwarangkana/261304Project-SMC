; Factorial Program for SMC Architecture 
; r1 = n (input), ผลลัพธ์สุดท้ายอยู่ใน r3 แล้วเก็บลง OUT

        lw      0       1       N       ; r1 = n
        lw      0       6       FACTAD  ; r6 = &FACTOR
        jalr    6       7               ; call factorial(n)
        sw      0       3       OUT     ; OUT = r3
        halt                            ; stop

; factorial: r3 = factorial(r1)
FACTOR  beq     1       0       BASE    ; if (n==0) goto BASE

        ; --- Save caller state ---
        sw      0       1       SAVEN   ; save n
        sw      0       7       SAVER7  ; save return address

        ; --- factorial(n-1) ---
        lw      0       2       NEG1    ; r2 = -1
        add     1       2       1       ; n = n - 1
        ; r6 ยังชี้ FACTOR อยู่แล้วจาก caller ไม่ต้องโหลดใหม่
        jalr    6       7               ; call factorial(n-1) → r3

        ; --- Restore state ---
        lw      0       1       SAVEN   ; restore n
        lw      0       7       SAVER7  ; restore return address

        ; --- r3 = n * factorial(n-1) using repeated-add ---
        add     0       3       4       ; r4 = factorial(n-1)
        add     0       0       3       ; r3 = 0
        add     0       1       2       ; r2 = n (counter)
        lw      0       5       NEG1    ; r5 = -1 (for counter--)

MULT    beq     2       0       MULTD   ; if counter==0 → done
        add     3       4       3       ; r3 += r4
        add     2       5       2       ; counter--
        beq     0       0       MULT    ; loop

MULTD   jalr    7       6               ; return to caller

; --- Base case: factorial(0) = 1 ---
BASE    lw      0       3       ONE     ; r3 = 1
        jalr    7       6               ; return

; --- Data section ---
N       .fill   3       ; input n (แก้ค่านี้ได้)
ONE     .fill   1
NEG1    .fill   -1
SAVEN   .fill   0
SAVER7  .fill   0
OUT     .fill   0
FACTAD  .fill   FACTOR  ; address of FACTOR
