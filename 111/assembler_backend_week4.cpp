// assembler_backend_week4.cpp
// เป้าหมาย (Part B - สัปดาห์ที่ 4):
//   1) เพิ่ม assembleProgram(...) : วน IR → แปลงเป็น machine code 32-bit
//   2) เขียนไฟล์เอาต์พุต "เลขฐาน 10" บรรทัดละ 1 ค่า ตามสเปก
//   3) จัดการสถานะออกจากโปรแกรม: สำเร็จ exit(0), ผิดพลาด exit(1)
//
// หมายเหตุ:
//   - โค้ด encode เหมือนสัปดาห์ที่ 3 (fixed/portable) แต่เพิ่มส่วน program driver
//   - ส่วน Parser (Part A) จะส่ง IR + symbol table จริงมาให้เราเรียก assembleProgram
//   - ใน main ด้านล่างทำ mock data ไว้เดโม่ (เปลี่ยนได้ตามการ integrate)
//
// Build & Run:
//   g++ -std=c++17 -O2 assembler_backend_week4.cpp -o asm_w4
//   ./asm_w4 machine_code.txt
//
// ถ้าไม่ส่งพาธไฟล์เอาต์พุต ทางโปรแกรมจะใช้ดีฟอลต์: "out.mc"

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <iomanip>
#include <cctype>
#include <stdexcept>
#include <cstdint>
#include <fstream>

using namespace std;

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

// -------------------- Helpers (จาก Week 1–2) --------------------
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
    if (!looksNumber(token))
        return {AsmError::BAD_IMMEDIATE, "not a valid number: " + token};
    try{
        size_t pos=0;
        outVal = stoll(token, &pos, 0); // base 0 => auto (0x..)
        if (pos!=token.size())
            return {AsmError::BAD_IMMEDIATE, "trailing junk: " + token};
        return {AsmError::NONE,""};
    }catch(...){
        return {AsmError::BAD_IMMEDIATE, "cannot parse: " + token};
    }
}

ErrInfo findLabel(const unordered_map<string,int>& symtab,
                  const string& label, int& outAddr){
    auto it = symtab.find(label);
    if (it==symtab.end()) return {AsmError::UNDEFINED_LABEL, "undefined label: " + label};
    outAddr = it->second; return {AsmError::NONE,""};
}

// asOffset16=true => ต้องเข้า 16 บิต; isBranch=true => beq relative offset
ErrInfo getFieldValue(const unordered_map<string,int>& symtab,
                      const string& token, int currentPC,
                      bool asOffset16, bool isBranch, int& outVal){
    long long val=0;
    if (looksNumber(token)){
        ErrInfo e = parseNumber(token,val);
        if(e.code!=AsmError::NONE) return e;
    }else{
        int addr=0; ErrInfo e = findLabel(symtab, token, addr);
        if (e.code!=AsmError::NONE) return e;
        if (isBranch) val = (long long)addr - (long long)(currentPC+1);
        else          val = addr;
    }
    if (asOffset16 && !inSigned16(val))
        return {AsmError::OFFSET_OUT_OF_RANGE, "offset out of 16-bit range: " + to_string(val)};
    outVal = (int)val; return {AsmError::NONE,""};
}

// -------------------- IR จาก Part A --------------------
struct IRInstr {
    string mnemonic;      // "add","lw","beq","jalr","halt","noop",".fill"
    int    regA{-1}, regB{-1}, dest{-1};
    string fieldToken;    // I-type/.fill
    int    pc{-1};        // address ของบรรทัดนี้ (เริ่ม 0)
};

// -------------------- Encode ทีละคำสั่ง (Week 3) --------------------
static ErrInfo needReg(int reg, const string& name){
    if (reg<0) return {AsmError::BAD_REGISTER, "missing "+name};
    if (!okReg(reg)) return {AsmError::BAD_REGISTER, "bad "+name+": "+to_string(reg)};
    return {AsmError::NONE,""};
}

struct EncodeResult {
    ErrInfo  error;
    int32_t  word{0};
};

EncodeResult assembleOne(const unordered_map<string,int>& symtab, const IRInstr& ir){
    EncodeResult r;
    int opcode=-1;
    r.error = toOpcode(ir.mnemonic, opcode);
    if (r.error.code!=AsmError::NONE) return r;

    if (opcode < 0) {
        // .fill → คืนค่าตรง ๆ (เลข/addr)
        int val=0;
        ErrInfo e = getFieldValue(symtab, ir.fieldToken, ir.pc, false, false, val);
        if (e.code!=AsmError::NONE){ r.error=e; return r; }
        r.word = val;
        return r;
    }

    switch (static_cast<Op>(opcode)){
        case Op::ADD:
        case Op::NAND: {
            ErrInfo e = needReg(ir.regA,"regA"); if (e.code!=AsmError::NONE){ r.error=e; return r; }
            e = needReg(ir.regB,"regB");         if (e.code!=AsmError::NONE){ r.error=e; return r; }
            e = needReg(ir.dest,"destReg");      if (e.code!=AsmError::NONE){ r.error=e; return r; }
            r.word = (int32_t)packR(opcode, ir.regA, ir.regB, ir.dest);
            return r;
        }
        case Op::LW:
        case Op::SW: {
            ErrInfo e = needReg(ir.regA,"regA"); if (e.code!=AsmError::NONE){ r.error=e; return r; }
            e = needReg(ir.regB,"regB");         if (e.code!=AsmError::NONE){ r.error=e; return r; }
            int off=0;
            e = getFieldValue(symtab, ir.fieldToken, ir.pc, true, false, off);
            if (e.code!=AsmError::NONE){ r.error=e; return r; }
            r.word = (int32_t)packI(opcode, ir.regA, ir.regB, off);
            return r;
        }
        case Op::BEQ: {
            ErrInfo e = needReg(ir.regA,"regA"); if (e.code!=AsmError::NONE){ r.error=e; return r; }
            e = needReg(ir.regB,"regB");         if (e.code!=AsmError::NONE){ r.error=e; return r; }
            int off=0;
            e = getFieldValue(symtab, ir.fieldToken, ir.pc, true, true, off);
            if (e.code!=AsmError::NONE){ r.error=e; return r; }
            r.word = (int32_t)packI(opcode, ir.regA, ir.regB, off);
            return r;
        }
        case Op::JALR: {
            ErrInfo e = needReg(ir.regA,"regA"); if (e.code!=AsmError::NONE){ r.error=e; return r; }
            e = needReg(ir.regB,"regB");         if (e.code!=AsmError::NONE){ r.error=e; return r; }
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

// -------------------- Week 4: assembleProgram + เขียนไฟล์ + exit code --------------------
// ความตั้งใจของฉัน:
//   - วนทุก IR → แปลงเป็น machine code
//   - ถ้าพบ error ที่บรรทัดไหน ให้รายงาน (stderr) พร้อม PC และสาเหตุ แล้วหยุดทันที exit(1)
//   - ถ้าสำเร็จทั้งหมด → เขียนไฟล์เลขฐาน 10 บรรทัดละ 1 ค่า แล้ว exit(0)
int assembleProgram(const unordered_map<string,int>& symtab,
                    const vector<IRInstr>& irs,
                    const string& outPath)
{
    ofstream out(outPath);
    if (!out) {
        cerr << "ERROR: cannot open output file: " << outPath << "\n";
        return 1;
    }

    for (size_t i=0; i<irs.size(); ++i) {
        const IRInstr& ir = irs[i];
        EncodeResult res = assembleOne(symtab, ir);
        if (res.error.code != AsmError::NONE) {
            cerr << "ERROR at PC=" << ir.pc
                 << " (" << ir.mnemonic << "): " << res.error.msg << "\n";
            // หยุดเขียนไฟล์และส่งสถานะล้มเหลว
            return 1;
        }
        // ตามสเปก: พิมพ์ "เลขฐาน 10" หนึ่งค่า/บรรทัด
        out << res.word << "\n";
        if (!out) {
            cerr << "ERROR: write failed at PC=" << ir.pc << "\n";
            return 1;
        }
    }

    // สำเร็จครบทุกบรรทัด
    return 0;
}

// -------------------- main (เดโม่): เปลี่ยนเป็นรับ IR จริงตอน integrate --------------------
int main(int argc, char** argv){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // ไฟล์เอาต์พุต (รับจาก argv[1]) หรือใช้ดีฟอลต์ out.mc
    string outPath = (argc >= 2 ? string(argv[1]) : string("out.mc"));

    // symbol table (จำลองจาก Part A)
    unordered_map<string,int> symtab{
        {"start",0}, {"loop",3}, {"data",10}, {"end",20}
    };

    // IR (เดโม่): ตอน integrate ให้เปลี่ยนเป็น IR จริงจาก Part A
    vector<IRInstr> irs = {
        {"add",  0,1,2, "",      0},       // add r0 r1 r2
        {"nand", 1,2,3, "",      1},       // nand r1 r2 r3
        {"lw",   1,2,-1,"data",  2},       // lw r1 r2 data
        {"sw",   1,2,-1,"0x7F",  3},       // sw r1 r2 0x7F
        {"beq",  1,2,-1,"loop",  4},       // beq r1 r2 loop
        {"jalr", 4,5,-1,"",      5},       // jalr r4 r5
        {"halt", -1,-1,-1,"",    6},       // halt
        {"noop", -1,-1,-1,"",    7},       // noop
        {".fill",-1,-1,-1,"123", 8},       // data literal
        {".fill",-1,-1,-1,"end", 9}        // data = address of 'end'
    };

    // เรียก assembleProgram และตัดสินใจ exit code ตามสเปก
    int code = assembleProgram(symtab, irs, outPath);

    if (code == 0) {
        cout << "Assemble success. Wrote machine code to: " << outPath << "\n";
    } else {
        cerr << "Assemble failed. See errors above.\n";
    }

    // สเปกกำหนดไว้ชัด: สำเร็จ exit(0), ล้มเหลว exit(1)
    // ถ้าเอาไปเชื่อมกับระบบเกรด/เทสเตอร์ จะได้ตรวจง่าย
    return code;
}
