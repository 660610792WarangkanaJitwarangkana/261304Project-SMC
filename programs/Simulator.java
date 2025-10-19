
import java.io.*;
import java.util.*;

class Main {

    public static void main(String[] args) throws Exception {
        Simulator.main(args);  // pass through "<machine_code>.mc"
    }
}

/**
 * SMC Simulator (8 regs, 32-bit, word-addressed) Usage: java Simulator
 * <machine-code.txt>
 *
 * - One signed 32-bit decimal per line in the input file. - numMemory = number
 * of lines. - Registers init to 0; R0 is always 0 (writes ignored). - PC starts
 * at 0. - printState is called before each instruction executes, and once more
 * before exit. - I-type offsets are 16-bit two's complement; sign-extended at
 * runtime. Opcodes (bits 24..22): add=000, nand=001, lw=010, sw=011, beq=100,
 * jalr=101, halt=110, noop=111
 */
public class Simulator {

    private static final int NUM_REGS = 8;
    private static final int NUMMEMORY = 65536;

    // Machine state
    private int pc = 0;
    private final int[] regs = new int[NUM_REGS];
    private final int[] mem = new int[NUMMEMORY];  // fixed-size memory (word-addressed)
    private int numMemory = 0;                      // number of loaded lines
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

    /**
     * Load machine code (one integer per line) from a file path
     */
    private void loadFromFile(String path) throws IOException {
        try (BufferedReader br = new BufferedReader(new FileReader(path))) {
            ArrayList<Integer> lines = new ArrayList<>();
            String s;
            int lineNo = 0;
            while ((s = br.readLine()) != null) {
                lineNo++;
                s = s.trim();
                if (s.isEmpty()) {
                    continue;                  // allow blank lines

                }
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
            for (int i = 0; i < numMemory; i++) {
                mem[i] = lines.get(i);
                // System.out.println("memory[" + i + "]=" + mem[i]);  // (optional echo)
            }
            Arrays.fill(regs, 0);
            regs[0] = 0; // enforce R0=0
        }
    }

    private static final int MAX_STEPS = 1_000_000;
    private int steps = 0;

    /**
     * Main simulation loop
     */
    private void run() {
        while (true) {
            printState(); // before executing each instruction

            // Step-limit guard (before fetch)
            if (++steps > MAX_STEPS) {
                System.err.println("possible infinite loop (exceeded " + MAX_STEPS + " steps)");
                halted = true;         // behave like halt: print once more and exit
                printState();
                break;
            }

            // Fetch
            if (pc < 0 || pc >= numMemory) {
                System.err.println("error: pc out of bounds: " + pc);
                halted = true;         // stop on invalid fetch
                printState();
                break;
            }
            int instr = mem[pc];

            // Decode
            int opcode = (instr >>> 22) & 0x7;
            int rs = (instr >>> 19) & 0x7;
            int rt = (instr >>> 16) & 0x7;
            int rd = instr & 0x7;     // R-type dest
            int imm16 = instr & 0xFFFF;  // I-type raw
            int imm32 = convertNum(imm16);        // sign-extend 16->32

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
                        halted = true;                 // <<< STOP on OOB
                        break;
                    }
                    regs[rt] = mem[addr];
                    break;
                }
                case 3: { // sw
                    int addr = regs[rs] + imm32;
                    if (addr < 0 || addr >= NUMMEMORY) {
                        System.err.println("error: sw address out of bounds: " + addr);
                        halted = true;                 // <<< STOP on OOB
                        break;
                    }
                    mem[addr] = regs[rt];
                    break;
                }
                case 4: { // beq
                    if (regs[rs] == regs[rt]) {
                        nextPC = pc + 1 + imm32; // offset is words
                    }
                    break;
                }
                case 5: { // jalr
                    int ret = pc + 1;       // compute return address first
                    int target = regs[rs];  // jump target (captured before writing link)
                    regs[rt] = ret;         // write link
                    nextPC = target;        // jump
                    break;
                }
                case 6: { // halt
                    halted = true;
                    break;
                }
                case 7: { // noop
                    // nothing
                    break;
                }
                default: {
                    System.err.println("error: unknown opcode " + opcode);
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

    /**
     * Sign-extend 16-bit value to 32-bit
     */
    private static int convertNum(int num) {
        if ((num & (1 << 15)) != 0) {
            num -= (1 << 16);
        }
        return num;
    }

    /**
     * Print machine state in the project required format
     */
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
        System.out.println("end state");
        System.out.println();
    }
}
