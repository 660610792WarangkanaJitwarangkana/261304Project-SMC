// assembler.cpp
// Build:
//   g++ -std=c++17 -O2 assembler.cpp -o assembler.exe
// Run (มีดีฟอลต์):
//   .\assembler.exe
// หรือส่งพาธไฟล์เอง:
//   .\assembler.exe program.ir symbolsTable.txt machineCode.mc
//
// จุดประสงค์ไฟล์ (Part B - สัปดาห์ที่ 3-4):
//   - Week3: เข้ารหัส IR เป็น machine code 32 บิต (R/I/J/O + .fill)
//   - Week4: วนประกอบทั้งโปรแกรม → เขียนไฟล์เอาต์พุตเลขฐาน 10 บรรทัดละ 1 ค่า
//            ถ้า error: รายงานแล้วหยุดทันที return 1, ถ้าสำเร็จ: return 0
//
// แนวคิดหลัก:
//   1) map mnemonic -> opcode ตามสเปก
//   2) แยกฟอร์แมต (R/I/J/O/.fill) แล้ว pack บิตด้วย shift/mask
//   3) ฟังก์ชันเสริม: หา label, แปลงเลข (dec/hex), คำนวณ offset (beq = target - (PC+1))
//   4) error handling: opcode ไม่รู้จัก, reg ผิดช่วง, offset 16-bit เกินช่วง, label ไม่มี
//   5) assembleProgram: ถ้าเจอ error ใด ๆ ให้หยุดทันที (fail-fast) และไม่เขียนไฟล์ต่อ

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <iomanip>
#include <cctype>
#include <stdexcept>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <algorithm>

using namespace std;

// ---------- ยูทิลพื้นฐานเล็ก ๆ ----------
static inline string rtrim(string s) {
    auto it = find_if(s.rbegin(), s.rend(), [](unsigned char ch){return !isspace(ch);});
    s.erase(it.base(), s.end());
    return s;
}
static inline string safeSubstr(const string& s, size_t pos, size_t len) {
    if (pos >= s.size()) return "";
    len = min(len, s.size() - pos);
    return s.substr(pos, len);
}
static bool tryParseInt(const string& s, int& out) {
    try {
        size_t p=0; long long v = stoll(rtrim(s), &p, 0);
        if (p != rtrim(s).size()) return false;
        if (v < INT32_MIN || v > INT32_MAX) return false;
        out = (int)v;
        return true;
    } catch (...) { return false; }
}

// -------------------- Opcodes --------------------
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

// -------------------- Bit layout --------------------
constexpr int OPCODE_SHIFT = 22;
constexpr int REGA_SHIFT   = 19;
constexpr int REGB_SHIFT   = 16;

inline uint32_t packR(int opcode, int rA, int rB, int dest) {
    return (uint32_t(opcode) << OPCODE_SHIFT)
         | (uint32_t(rA)     << REGA_SHIFT)
         | (uint32_t(rB)     << REGB_SHIFT)
         | (uint32_t(dest) & 0x7u);
}
inline uint32_t packI(int opcode, int rA, int rB, int offset16) {
    return (uint32_t(opcode) << OPCODE_SHIFT)
         | (uint32_t(rA)     << REGA_SHIFT)
         | (uint32_t(rB)     << REGB_SHIFT)
         | (uint32_t(offset16) & 0xFFFFu);
}
inline uint32_t packJ(int opcode, int rA, int rB) {
    return (uint32_t(opcode) << OPCODE_SHIFT)
         | (uint32_t(rA)     << REGA_SHIFT)
         | (uint32_t(rB)     << REGB_SHIFT);
}
inline uint32_t packO(int opcode) {
    return (uint32_t(opcode) << OPCODE_SHIFT);
}

// -------------------- Errors --------------------
enum class AsmError {
    NONE = 0,
    UNKNOWN_OPCODE,
    UNDEFINED_LABEL,
    OFFSET_OUT_OF_RANGE,
    BAD_IMMEDIATE,
    BAD_REGISTER
};
struct ErrInfo {
    AsmError code{AsmError::NONE};
    string   msg;
};
inline bool okReg(int r){ return 0<=r && r<=7; } // reg 3 บิต: 0..7

// -------------------- Helper: mapping/parse/symbol/offset --------------------
ErrInfo toOpcode(const string& mnemonic, int& outOpcode) {
    auto it = OPCODE_MAP.find(mnemonic);
    if (it == OPCODE_MAP.end())
        return {AsmError::UNKNOWN_OPCODE, "unknown opcode: " + mnemonic};
    if (it->second == Op::FILL) { outOpcode = -1; return {AsmError::NONE,""}; }
    outOpcode = static_cast<int>(it->second);
    return {AsmError::NONE,""};
}

inline bool inSigned16(long long x){ return -32768<=x && x<=32767; }

bool looksNumber(const string& s){
    if (s.empty()) return false;
    size_t i=0; if (s[0]=='+'||s[0]=='-') i=1;
    if (i>=s.size()) return false;
    if (i+1<s.size() && s[i]=='0' && (s[i+1]=='x'||s[i+1]=='X')){
        i+=2; if (i>=s.size()) return false;
        for(; i<s.size(); ++i) if(!isxdigit((unsigned char)s[i])) return false;
        return true;
    }
    for(; i<s.size(); ++i) if(!isdigit((unsigned char)s[i])) return false;
    return true;
}
ErrInfo parseNumber(const string& token, long long& outVal){
    string t = rtrim(token);
    if (!looksNumber(t))
        return {AsmError::BAD_IMMEDIATE, "not a valid number: " + t};
    try{
        size_t pos=0;
        outVal = stoll(t, &pos, 0); // base 0 => auto (0x..)
        if (pos!=t.size())
            return {AsmError::BAD_IMMEDIATE, "trailing junk: " + t};
        return {AsmError::NONE,""};
    }catch(...){
        return {AsmError::BAD_IMMEDIATE, "cannot parse: " + t};
    }
}

ErrInfo findLabel(const unordered_map<string,int>& symtab,
                  const string& label, int& outAddr){
    auto it = symtab.find(label);
    if (it==symtab.end()) return {AsmError::UNDEFINED_LABEL, "undefined label: " + label};
    outAddr = it->second; return {AsmError::NONE,""};
}

// คืนค่าฟิลด์ (เลข/label) โดยคำนึงถึงชนิดฟิลด์
ErrInfo getFieldValue(const unordered_map<string,int>& symtab,
                      const string& token, int currentPC,
                      bool asOffset16, bool isBranch, int& outVal){
    long long val=0;
    string t = rtrim(token);
    if (looksNumber(t)){
        ErrInfo e = parseNumber(t,val);
        if(e.code!=AsmError::NONE) return e;
    }else{
        int addr=0; ErrInfo e = findLabel(symtab, t, addr);
        if (e.code!=AsmError::NONE) return e;
        if (isBranch) val = (long long)addr - (long long)(currentPC+1);
        else          val = addr; // lw/sw/.fill ใช้ address ตรง ๆ
    }
    if (asOffset16 && !inSigned16(val))
        return {AsmError::OFFSET_OUT_OF_RANGE, "offset out of 16-bit range: " + to_string(val)};
    outVal = (int)val;
    return {AsmError::NONE,""};
}

// -------------------- โครงสร้าง IR (อินพุตจาก Part A) --------------------
struct IRInstr {
    string mnemonic;      // "add","lw","beq","jalr","halt","noop",".fill"
    int    regA{-1}, regB{-1}, dest{-1}; // ใช้เฉพาะบาง format
    string fieldToken;    // ใช้กับ I-type หรือ .fill
    int    pc{-1};        // address ของบรรทัดนี้ (เริ่ม 0)
};

// -------------------- ฟังก์ชันช่วยเช็คเรจิสเตอร์ --------------------
static ErrInfo needReg(int reg, const string& name){
    if (reg < 0)
        return {AsmError::BAD_REGISTER, "missing "+name};
    if (!okReg(reg))
        return {AsmError::BAD_REGISTER, "bad "+name+": "+to_string(reg)};
    return {AsmError::NONE,""};
}

// -------------------- เข้ารหัสทีละคำสั่ง (Week 3) --------------------
struct EncodeResult {
    ErrInfo  error;
    int32_t  word{0};  // machine code (พิมพ์เป็นฐาน 10)
};

EncodeResult assembleOne(const unordered_map<string,int>& symtab, const IRInstr& ir){
    EncodeResult r;
    int opcode=-1;

    // 1) แปลง mnemonic -> opcode (หรือ .fill = -1)
    r.error = toOpcode(ir.mnemonic, opcode);
    if (r.error.code!=AsmError::NONE) return r;

    // 2) .fill = ไม่ใช่คำสั่ง → คืนค่า literal/address ตรง ๆ
    if (opcode < 0){
        int val=0;
        ErrInfo e = getFieldValue(symtab, ir.fieldToken, ir.pc, false, false, val);
        if (e.code!=AsmError::NONE){ r.error=e; return r; }
        r.word = val;
        return r;
    }

    // 3) เลือก pack ตามชนิด
    switch (static_cast<Op>(opcode)){
        case Op::ADD:
        case Op::NAND: {
            ErrInfo e = needReg(ir.regA, "regA"); if (e.code!=AsmError::NONE){ r.error=e; return r; }
            e = needReg(ir.regB, "regB");         if (e.code!=AsmError::NONE){ r.error=e; return r; }
            e = needReg(ir.dest, "destReg");      if (e.code!=AsmError::NONE){ r.error=e; return r; }
            r.word = (int32_t)packR(opcode, ir.regA, ir.regB, ir.dest);
            return r;
        }
        case Op::LW:
        case Op::SW: {
            ErrInfo e = needReg(ir.regA, "regA"); if (e.code!=AsmError::NONE){ r.error=e; return r; }
            e = needReg(ir.regB, "regB");         if (e.code!=AsmError::NONE){ r.error=e; return r; }
            int off=0;
            e = getFieldValue(symtab, ir.fieldToken, ir.pc, true, false, off);
            if (e.code!=AsmError::NONE){ r.error=e; return r; }
            r.word = (int32_t)packI(opcode, ir.regA, ir.regB, off);
            return r;
        }
        case Op::BEQ: {
            ErrInfo e = needReg(ir.regA, "regA"); if (e.code!=AsmError::NONE){ r.error=e; return r; }
            e = needReg(ir.regB, "regB");         if (e.code!=AsmError::NONE){ r.error=e; return r; }
            int off=0;
            e = getFieldValue(symtab, ir.fieldToken, ir.pc, true, true, off);
            if (e.code!=AsmError::NONE){ r.error=e; return r; }
            r.word = (int32_t)packI(opcode, ir.regA, ir.regB, off);
            return r;
        }
        case Op::JALR: {
            ErrInfo e = needReg(ir.regA, "regA"); if (e.code!=AsmError::NONE){ r.error=e; return r; }
            e = needReg(ir.regB, "regB");         if (e.code!=AsmError::NONE){ r.error=e; return r; }
            r.word = (int32_t)packJ(opcode, ir.regA, ir.regB);
            return r;
        }
        case Op::HALT:
        case Op::NOOP: {
            r.word = (int32_t)packO(opcode);
            return r;
        }
        default:
            r.error = {AsmError::UNKNOWN_OPCODE, "unhandled opcode"};
            return r;
    }
}

// -------------------- Week 4: assembleProgram (fail-fast + เขียนไฟล์) --------------------
int assembleProgram(const unordered_map<string,int>& symtab,
                    const vector<IRInstr>& irs,
                    const string& outPath)
{
    ofstream out(outPath);
    if (!out) {
        cerr << "ERROR: cannot open output file: " << outPath << "\n";
        return 1;
    }

    for (const auto& ir : irs) {
        EncodeResult res = assembleOne(symtab, ir);
        if (res.error.code != AsmError::NONE) {
            // รายงานข้อผิดพลาดพร้อม PC และคำสั่งที่ทำอยู่
            cerr << "ERROR at PC=" << ir.pc
                 << " (" << ir.mnemonic << "): " << res.error.msg << "\n";
            return 1; // หยุดทันทีตามสเปก
        }
        // เขียนเลขฐาน 10 บรรทัดละ 1 ค่า
        out << res.word << "\n";
        if (!out) {
            cerr << "ERROR: write failed at PC=" << ir.pc << "\n";
            return 1;
        }

        cout << "(address " << ir.pc << "): " 
             << res.word << " (hex 0x"
             << hex << uppercase << setw(0)
             << ((uint32_t)res.word & 0xFFFFFFFF)
             << dec << ")\n";
    }
    
    return 0; // สำเร็จครบทุกบรรทัด
}

// -------------------- โหลด Symbols & IR (เวอร์ชันทนทานกว่าเดิม) --------------------
unordered_map<string,int> loadSymbolTable(const string& filename){
    unordered_map<string,int> symbols;
    ifstream fin(filename);
    if(!fin){ cerr<<"ERROR: cannot open symbols file: "<<filename<<"\n"; return symbols; }

    string line;
    // ข้าม header ถ้าบรรทัดแรกเป็นหัวตาราง
    getline(fin, line);

    while (getline(fin, line)) {
        if (line.empty()) continue;
        istringstream iss(line);
        string label; int addr;
        if (iss >> label >> addr)
            symbols[label] = addr;
        // ถ้า parse ไม่ได้ก็ข้าม (หรือจะแจ้งเตือนเพิ่มก็ได้)
    }

    cerr << "Loaded " << symbols.size() << " symbol(s) from: " << filename << "\n";
    return symbols;
}

// หมายเหตุ: ช่องคอลัมน์ตามฟอร์แมตเดิมของคุณ
// [0..7]=addr, [8..15]=label, [16..23]=instr, [24..31]=f0, [32..39]=f1, [40..47]=f2,
// [48..55]=regA, [56..63]=regB, [64..71]=dest, [72..79]=off, [80..]=fill
vector<IRInstr> loadIR(const string& filename){
    vector<IRInstr> IR;
    ifstream fin(filename);
    if(!fin){ cerr<<"ERROR: cannot open IR file: "<<filename<<"\n"; return IR; }

    string line;
    getline(fin, line); // header

    while (getline(fin, line)) {
        if (line.empty()) continue;

        IRInstr ir;

        string addrStr = safeSubstr(line, 0, 8);   ir.pc = tryParseInt(addrStr, ir.pc) ? ir.pc : 0;
        string label   = rtrim(safeSubstr(line, 8, 8));
        string instr   = rtrim(safeSubstr(line,16, 8));
        string f0      = rtrim(safeSubstr(line,24, 8));
        string f1      = rtrim(safeSubstr(line,32, 8));
        string f2      = rtrim(safeSubstr(line,40, 8));

        string regAstr = safeSubstr(line,48, 8);
        string regBstr = safeSubstr(line,56, 8);
        string destr   = safeSubstr(line,64, 8);
        string offstr  = safeSubstr(line,72, 8);
        string fillstr = safeSubstr(line,80, 16);

        int tmp;
        if (tryParseInt(regAstr, tmp)) ir.regA = tmp; else ir.regA = -1;
        if (tryParseInt(regBstr, tmp)) ir.regB = tmp; else ir.regB = -1;
        if (tryParseInt(destr,   tmp)) ir.dest = tmp; else ir.dest = -1;

        // ตัดสินใจ fieldToken:
        //  - .fill → ใช้ f0 หรือ fillstr (เลือก f0 ตามฟอร์แมตตารางของคุณ)
        //  - lw/sw/beq → ใช้ f2 (label/imm)
        //  - อื่น ๆ → ไม่ใช้ fieldToken
        if (instr == ".fill") {
            ir.fieldToken = !f0.empty() ? f0 : rtrim(fillstr);
        } else if (instr == "lw" || instr == "sw" || instr == "beq") {
            ir.fieldToken = f2;
        } else {
            ir.fieldToken.clear();
        }

        ir.mnemonic = instr;
        IR.push_back(ir);
    }

    cerr << "Loaded " << IR.size() << " IR row(s) from: " << filename << "\n";
    return IR;
}

// -------------------- main: glue ทั้งหมด + exit code ตามสเปก --------------------
int main(int argc, char** argv){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // ดีฟอลต์ชื่อไฟล์ (เปลี่ยนได้จาก argv)
    string irPath  = (argc >= 2 ? argv[1] : "program.ir");
    string symPath = (argc >= 3 ? argv[2] : "symbolsTable.txt");
    string outPath = (argc >= 4 ? argv[3] : "machineCode.mc");

    // โหลดสัญลักษณ์และ IR
    auto symtab = loadSymbolTable(symPath);
    auto irs    = loadIR(irPath);

    // ถ้าไม่มี IR เลย ให้ถือว่า error (ไม่มีอะไรให้ assemble)
    if (irs.empty()) {
        cerr << "ERROR: no IR to assemble (check " << irPath << ")\n";
        return 1;
    }

    cerr << "--- Assembling " << irs.size() << " instruction(s) ---\n";

    // เรียก assembleProgram (fail-fast + เขียนไฟล์ภายใน)
    int code = assembleProgram(symtab, irs, outPath);

    if (code == 0) {
        cout << "Assemble success. Wrote machine code to: " << outPath << "\n";
    } else {
        cerr << "Assemble failed. See errors above.\n";
    }
    return code; // 0=สำเร็จ, 1=ล้มเหลว
}
