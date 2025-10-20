        lw      0       1       five    ; โหลดค่า 5 จาก address 9 ไปยัง register 1
        lw      0       2       three   ; โหลดค่า 3 จาก address 10 ไปยัง register 2
        lw      0       4       neg1    ; โหลดค่า -1 จาก address 11 ไปยัง register 4
        add     0       0       5       ; ตั้งค่า register 5 = 0 (ผลลัพธ์)
        
loop    beq     2       0       done    ; ถ้า register 2 == 0 กระโดดไปที่ done
        add     5       1       5       ; ผลลัพธ์ = ผลลัพธ์ + 5
        add     2       4       2       ; ตัวนับ = ตัวนับ - 1
        beq     0       0       loop    ; กระโดดกลับไปที่ loop
        
done    halt                            ; จบโปรแกรม

five    .fill   5
three   .fill   3
neg1    .fill   -1