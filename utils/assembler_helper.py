# utils/assembler_helper.py
opcode_table = {
    "add": 0, "nand": 1, "lw": 2, "sw": 3,
    "beq": 4, "jalr": 5, "halt": 6, "noop": 7
}

def encode_rtype(op, rs, rt, rd):
    return (opcode_table[op] << 22) | (rs << 19) | (rt << 16) | rd

def encode_itype(op, rs, rt, imm):
    return (opcode_table[op] << 22) | (rs << 19) | (rt << 16) | (imm & 0xFFFF)

def assemble_line(line):
    parts = line.strip().split()
    if len(parts) == 0 or parts[0].startswith(";"):
        return None
    op = parts[0]
    if op == "add":
        return encode_rtype(op, int(parts[1]), int(parts[2]), int(parts[3]))
    elif op in ["lw", "sw", "beq"]:
        return encode_itype(op, int(parts[1]), int(parts[2]), int(parts[3]))
    elif op in ["halt", "noop"]:
        return opcode_table[op] << 22
    else:
        raise ValueError(f"Unknown instruction: {op}")

def assemble_file(infile, outfile):
    with open(infile) as f, open(outfile, "w") as out:
        for line in f:
            code = assemble_line(line)
            if code is not None:
                out.write(str(code) + "\n")

if __name__ == "__main__":
    assemble_file("../programs/multiply.asm", "../programs/multiply.mc")
