            int opcode = (instr >>> 22) & 0x7;
            int rs     = (instr >>> 19) & 0x7;
            int rt     = (instr >>> 16) & 0x7;
            int rd     =  instr         & 0x7;     // R-type dest
            int imm16  =  instr         & 0xFFFF;  // I-type raw
            int imm32  = convertNum(imm16);        // sign-extend 16->32

            int nextPC = pc + 1; // default advance

            switch (opcode) {
                case 0: { // add
                    regs[rd] = regs[rs] + regs[rt];
                    break;
                }
                case 1: { // nand
                    regs[rd] = ~(regs[rs] & regs[rt]);
                    break;
                }
                case 2: { // lw
                    int addr = regs[rs] + imm32;
                        if (addr < 0 || addr >= NUMMEMORY) {
                            System.err.println("error: lw address out of bounds: " + addr);
                            break;
                        }
                    regs[rt] = mem[addr];
                }
                case 3: { // sw
                    int addr = regs[rs] + imm32;
                        if (addr < 0 || addr >= NUMMEMORY) {
                            System.err.println("error: sw address out of bounds: " + addr);
                            break;
                        }
                    mem[addr] = regs[rt];
                }
                case 4: { // beq
                    if (regs[rs] == regs[rt]) {
                        nextPC = pc + 1 + imm32; // offset is words; no shifting
                    }
                    break;
                }
                case 5: { // jalr
                    int ret = pc + 1;       // compute return address first
                    int target = regs[rs];  // jump target
                    regs[rt] = ret;         // write link
                    // Special case: rs==rt → jump to PC+1 (per spec)
                    nextPC = (rs == rt) ? ret : target;
                    break;
                }
                case 6: { // halt
                    halted = true;
                    // nextPC remains pc+1 (spec)
                    break;
                }
                case 7: { // noop
                    // nothing
                    break;
                }
                default: {
                    // Unknown opcode 
                    halted = true;
                    break;
                }
            }