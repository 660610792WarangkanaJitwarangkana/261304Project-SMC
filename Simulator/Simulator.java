import java.io.*;
import java.util.*;

class Main {
    public static void main(String[] args) throws Exception {
        Simulator.main(args);  // pass through "machine_code.txt"
    }
}

/**
 * SMC Simulator (8 regs, 32-bit, word-addressed) — filename via main(args)
 * Usage: java Simulator <machine-code.txt>
 *
 * - One signed 32-bit decimal per line in the input file.
 * - numMemory = number of lines.
 * - Registers init to 0; R0 is always 0 (writes ignored).
 * - PC starts at 0.
 * - printState is called before each instruction executes, and once more before exit.
 * - I-type offsets are 16-bit two's complement; sign-extended at runtime.
 * Opcodes (bits 24..22): add=000, nand=001, lw=010, sw=011, beq=100, jalr=101, halt=110, noop=111
 */

//  usage
//     - java Simulator <factorial.mc> 
//     - java Simulator factorial.mc > new file name <fact_sim.txt>

public class Simulator {

    private static final int NUM_REGS = 8;

    // Machine state
    private int pc = 0;
    private int[] regs = new int[NUM_REGS];
    private static final int NUMMEMORY = 65536;
    private int[] mem = new int[NUMMEMORY];  // instead of sizing to numMemory
    private int numMemory = 0;               // still track how many lines you loaded
    private boolean halted = false;

    public static void main(String[] args) throws Exception {
        if (args.length != 1) {
            System.err.println("error: usage: java Simulator <machine-code file>");
            System.exit(1);
        }
        Simulator sim = new Simulator();
        sim.loadFromFile(args[0]);
        sim.run();
    }

    /** Load machine code (one integer per line) from a file path */
    private void loadFromFile(String path) throws IOException {
        try (BufferedReader br = new BufferedReader(new FileReader(path))) {
            ArrayList<Integer> lines = new ArrayList<>();
            String s;
            int lineNo = 0;
            while ((s = br.readLine()) != null) {
                lineNo++;
                s = s.trim();
                if (s.isEmpty()) continue;                  // allow blank lines
                String first = s.split("\\s+")[0];          // tolerate trailing comments/tokens
                try {
                    int val = Integer.parseInt(first);
                    lines.add(val);
                } catch (NumberFormatException nfe) {
                    System.err.println("error in reading address " + (lines.size()) + " (line " + lineNo + ")");
                    System.exit(1);
                }
            }
            numMemory = lines.size();
            //mem = new int[numMemory];
            for (int i = 0; i < numMemory; i++) {
                mem[i] = lines.get(i);
                System.out.println("memory[" + i + "]=" + mem[i]);  // <— echo 
                }
                      Arrays.fill(regs, 0);
            regs[0] = 0; // enforce R0=0
        }
    }

    /** Main simulation loop */
    private void run() {
        while (true) {
            printState(); // before executing each instruction

            // Fetch
            if (pc < 0 || pc >= numMemory) {
                // Out-of-bounds fetch: stop (optional safety)
                System.err.println("error: pc out of bounds: " + pc);
                break;
            }
            int instr = mem[pc];

            // Decode
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
                    break;
                }
                case 3: { // sw
                    int addr = regs[rs] + imm32;
                        if (addr < 0 || addr >= NUMMEMORY) {
                            System.err.println("error: sw address out of bounds: " + addr);
                            break;
                        }
                    mem[addr] = regs[rt];
                    break;
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

            // Enforce R0 is always 0
            regs[0] = 0;
            // Commit PC
            pc = nextPC;

            // On halt: print state once more and exit
            if (halted) {
                printState();
                break;
            }
        }
    }

    /** Sign-extend 16-bit value to 32-bit */
    private static int convertNum(int num) {
        if ((num & (1 << 15)) != 0) {
            num -= (1 << 16);
        }
        return num;
    }

    /** Print machine state in the project’s required format */
    private void printState() {
        System.out.println("@@@");
        System.out.println("state:");
        System.out.println("\tpc " + pc);
        System.out.println("\tmemory:");
        for (int i = 0; i < numMemory; i++) {
            System.out.println("\t\tmem[ " + i + " ] " + mem[i]);
        }
        System.out.println("\tregisters:");
        for (int i = 0; i < NUM_REGS; i++) {
            System.out.println("\t\treg[ " + i + " ] " + regs[i]);
        }
        //end state
        System.out.println("end state");
        System.out.println(" ");
    }
}
