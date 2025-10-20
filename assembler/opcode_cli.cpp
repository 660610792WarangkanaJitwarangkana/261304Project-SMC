#include <bits/stdc++.h>
using namespace std;

enum class Op : int {
    ADD=0, NAND=1, LW=2, SW=3, BEQ=4, JALR=5, HALT=6, NOOP=7, FILL=-1
};
static const unordered_map<string, Op> OPCODE_MAP = {
    {"add",  Op::ADD},  {"nand", Op::NAND},
    {"lw",   Op::LW},   {"sw",   Op::SW},
    {"beq",  Op::BEQ},  {"jalr", Op::JALR},
    {"halt", Op::HALT}, {"noop", Op::NOOP},
    {".fill",Op::FILL}
};

int main(int argc, char** argv){
    if (argc != 2) {
        cerr << "usage: opcode <mnemonic>\n";
        cerr << "ex:    opcode add\n";
        return 1;
    }
    string m = argv[1];
    auto it = OPCODE_MAP.find(m);
    if (it == OPCODE_MAP.end()) {
        cout << "unknown mnemonic: " << m << "\n";
        return 1;
    }
    int opcode = static_cast<int>(it->second);
    cout << m << " -> opcode " << opcode;
    if (opcode >= 0) {
        // 3-bit binary
        cout << " (bin " << bitset<3>(opcode) << ")";
    }
    cout << "\n";
    return 0;
}
