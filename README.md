# 261304Project-SMC

## Structure
- assembler/: แปลง assembly เป็น machine code  
- simulator/: จำลองการทำงานของโปรแกรม  
- programs/: เก็บโปรแกรม multiply, factorial และ test case  
- utils/: สคริปต์เสริมของฉัน  

## Run
```bash
python utils/assembler_helper.py      # แปลง .asm เป็น .mc
java -cp .\Simulator Simulator ".\programs asm\factorialmem.mc"   # รัน simulator
