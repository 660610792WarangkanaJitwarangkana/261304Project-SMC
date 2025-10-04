# 261304Project-SMC

## Structure
- assembler/: แปลง assembly เป็น machine code  
- simulator/: จำลองการทำงานของโปรแกรม  
- programs/: เก็บโปรแกรม multiply, factorial และ test case  
- utils/: สคริปต์เสริมของฉัน  

## Run
```bash
python utils/assembler_helper.py      # แปลง .asm เป็น .mc
python simulator/simulator.py programs/multiply.mc   # รัน simulator
