// assembler.cpp
// ------------------------------------------------------------
// Build:
//   g++ -std=c++17 -O2 assembler.cpp -o assembler.exe
// Run (มีดีฟอลต์):
//   .\assembler.exe
// หรือส่งพาธไฟล์เอง:
//   .\assembler.exe program.ir symbolsTable.txt machineCode.mc
//
// จุดประสงค์ไฟล์ (Part B - สัปดาห์ที่ 3-4):
//   - Week 3: เข้ารหัส IR เป็น machine code 32 บิต (R/I/J/O + .fill)
//   - Week 4: วนประกอบทั้งโปรแกรม → เขียนไฟล์เอาต์พุตเลขฐาน 10 บรรทัดละ 1 ค่า
//             ถ้า error: รายงานแล้วหยุดทันที return 1, ถ้าสำเร็จ: return 0
//
// แนวคิดหลัก:
//   1) map mnemonic -> opcode ตามสเปก (add=0, nand=1, ... , noop=7, .fill=พิเศษ)
//   2) แยกฟอร์แมต (R/I/J/O/.fill) แล้ว pack บิตด้วย shift/mask ให้ตรง layout 32-bit
//   3) ฟังก์ชันเสริม: หา label, แปลงเลข (dec/hex), คำนวณ offset (beq = target - (PC+1))
//   4) error handling: opcode ไม่รู้จัก, reg ผิดช่วง, offset 16-bit เกินช่วง, label ไม่มี
//   5) assembleProgram: ถ้าเจอ error ใด ๆ ให้หยุดทันที (fail-fast) เพื่อไม่ให้ได้ไฟล์ผิด
// ------------------------------------------------------------

#include <iostream>      // พิมพ์ข้อความ/ผลลัพธ์พื้นฐาน
#include <string>        // std::string
#include <vector>        // std::vector
#include <unordered_map> // symbol table: ชื่อ label -> address
#include <iomanip>       // setw สำหรับจัด format แสดงผล
#include <cctype>        // isdigit/isxdigit ใช้ช่วยตรวจเลข
#include <stdexcept>     // รองรับ exception ตอนแปลงเลข
#include <cstdint>       // int32_t/uint32_t ใช้กับบิต 32 บิต
#include <fstream>       // อ่าน/เขียนไฟล์
#include <sstream>       // istringstream ใช้ parse บรรทัด
#include <algorithm>     // find_if ใช้ trim ด้านขวา

using namespace std;

// ---------- ยูทิลพื้นฐานเล็ก ๆ (คุณภาพชีวิต) ----------

// rtrim: ตัดช่องว่างขวาสุดของสตริง (กันปัญหาช่องว่างเจือปนจากไฟล์คอลัมน์คงที่)
static inline string rtrim(string s) {
    auto it = find_if(s.rbegin(), s.rend(), [](unsigned char ch){return !isspace(ch);});
    s.erase(it.base(), s.end());
    return s;
}

// safeSubstr: substr แบบปลอดภัย (ถ้า pos เกินความยาว ให้คืน "" เพื่อกัน out_of_range)
static inline string safeSubstr(const string& s, size_t pos, size_t len) {
    if (pos >= s.size()) return "";
    len = min(len, s.size() - pos);
    return s.substr(pos, len);
}

// tryParseInt: แปลงสตริงเป็น int (รองรับฐาน 10/16 แบบ auto) ถ้าพังให้ false
static bool tryParseInt(const string& s, int& out) {
    try {
        size_t p=0; long long v = stoll(rtrim(s), &p, 0); // base 0 => auto (0x.. เป็น hex)
        if (p != rtrim(s).size()) return false;           // มีขยะตามท้าย → ไม่เอา
        if (v < INT32_MIN || v > INT32_MAX) return false; // เกินช่วง int 32
        out = (int)v;
        return true;
    } catch (...) { return false; }
}

// -------------------- Opcodes และ mapping --------------------
// หมายเหตุ: .fill เป็น "directive" ไม่ใช่ instruction จึง set เป็น -1
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

// -------------------- Bit layout ของคำสั่ง 32 บิต --------------------
// เราใช้การ shift บิตเข้าตำแหน่งที่สเปกกำหนด:
// [ opcode(3) | regA(3) | regB(3) | (ที่เหลือ 16 บิต/3 บิตขึ้นกับชนิด) ]
constexpr int OPCODE_SHIFT = 22;
constexpr int REGA_SHIFT   = 19;
constexpr int REGB_SHIFT   = 16;

// ฟังก์ชัน pack บิตสำหรับแต่ละฟอร์แมต (ลด duplicate โค้ด)
inline uint32_t packR(int opcode, int rA, int rB, int dest) {
    // R-type: ช่องท้ายสุดใช้เพียง 3 บิตสำหรับ destReg
    return (uint32_t(opcode) << OPCODE_SHIFT)
        | (uint32_t(rA)     << REGA_SHIFT)
        | (uint32_t(rB)     << REGB_SHIFT)
        | (uint32_t(dest) & 0x7u);
}
inline uint32_t packI(int opcode, int rA, int rB, int offset16) {
    // I-type: 16 บิตท้ายใช้เก็บ offset แบบ two's complement
    return (uint32_t(opcode) << OPCODE_SHIFT)
        | (uint32_t(rA)     << REGA_SHIFT)
        | (uint32_t(rB)     << REGB_SHIFT)
        | (uint32_t(offset16) & 0xFFFFu); // mask 16 บิตล่างให้ชัดเจน
}
inline uint32_t packJ(int opcode, int rA, int rB) {
    // J-type: ใช้แค่ opcode + regA + regB, ที่เหลือ (16 บิต) เป็น 0
    return (uint32_t(opcode) << OPCODE_SHIFT)
        | (uint32_t(rA)     << REGA_SHIFT)
        | (uint32_t(rB)     << REGB_SHIFT);
}
inline uint32_t packO(int opcode) {
    // O-type: ใช้เฉพาะ opcode, ที่เหลือทั้งหมดเป็น 0
    return (uint32_t(opcode) << OPCODE_SHIFT);
}

// -------------------- โครงสร้าง error ที่เราจะรายงาน --------------------
enum class AsmError {
    NONE = 0,
    UNKNOWN_OPCODE,   // คำสั่งไม่อยู่ใน mapping
    UNDEFINED_LABEL,  // อ้าง label ที่ไม่มีใน symbol table
    OFFSET_OUT_OF_RANGE, // offset 16-bit เกินช่วง signed (-32768..32767)
    BAD_IMMEDIATE,    // immediate/number แปลงไม่ได้หรือรูปแบบผิด
    BAD_REGISTER      // เรจิสเตอร์หายไป หรืออยู่นอกช่วง 0..7 (3 บิต)
};
struct ErrInfo {
    AsmError code{AsmError::NONE};
    string   msg;
};

inline bool okReg(int r){ return 0<=r && r<=7; } // reg 3 บิต: 0..7 เท่านั้น

// -------------------- Helper: mapping/parse/symbol/offset --------------------

// แปลง mnemonic → opcode (int) (.fill = -1)
ErrInfo toOpcode(const string& mnemonic, int& outOpcode) {
    auto it = OPCODE_MAP.find(mnemonic);
    if (it == OPCODE_MAP.end())
        return {AsmError::UNKNOWN_OPCODE, "unknown opcode: " + mnemonic};
    if (it->second == Op::FILL) { outOpcode = -1; return {AsmError::NONE,""}; }
    outOpcode = static_cast<int>(it->second);
    return {AsmError::NONE,""};
}

// helper เช็คว่า x อยู่ในช่วง signed 16-bit หรือไม่
inline bool inSigned16(long long x){ return -32768<=x && x<=32767; }

// ตรวจ string ว่าเป็น “เลข” มั้ย (รองรับ +/-, 0x..)
bool looksNumber(const string& s){
    if (s.empty()) return false;
    size_t i=0; if (s[0]=='+'||s[0]=='-') i=1;
    if (i>=s.size()) return false;

    // รูปแบบ 0x.. (hex)
    if (i+1<s.size() && s[i]=='0' && (s[i+1]=='x'||s[i+1]=='X')){
        i+=2; if (i>=s.size()) return false;
        for(; i<s.size(); ++i) if(!isxdigit((unsigned char)s[i])) return false;
        return true;
    }

    // รูปแบบตัวเลขฐาน 10
    for(; i<s.size(); ++i) if(!isdigit((unsigned char)s[i])) return false;
    return true;
}

// แปลงสตริง → long long (ฐานอัตโนมัติ) พร้อมตรวจขยะตามท้าย
ErrInfo parseNumber(const string& token, long long& outVal){
    string t = rtrim(token);
    if (!looksNumber(t))
        return {AsmError::BAD_IMMEDIATE, "not a valid number: " + t};
    try{
        size_t pos=0;
        outVal = stoll(t, &pos, 0); // base 0: 0x..=hex, อื่น ๆ=dec
        if (pos!=t.size())
            return {AsmError::BAD_IMMEDIATE, "trailing junk: " + t};
        return {AsmError::NONE,""};
    }catch(...){
        return {AsmError::BAD_IMMEDIATE, "cannot parse: " + t};
    }
}

// หา address ของ label จาก symbol table (ไม่เจอ → error)
ErrInfo findLabel(const unordered_map<string,int>& symtab,
                const string& label, int& outAddr){
    auto it = symtab.find(label);
    if (it==symtab.end()) return {AsmError::UNDEFINED_LABEL, "undefined label: " + label};
    outAddr = it->second; return {AsmError::NONE,""};
}

// แปลงค่า field (ซึ่งอาจเป็นตัวเลขหรือ label) ให้อยู่ในรูป int พร้อมเงื่อนไข:
//   - asOffset16=true  → ต้องเข้า 16 บิต (ใช้กับ I-type)
//   - isBranch=true    → beq: offset = labelAddr - (PC+1) (relative)
//   - isBranch=false   → lw/sw/.fill: ใช้ “address ตรง ๆ” ถ้าเป็น label
ErrInfo getFieldValue(const unordered_map<string,int>& symtab,
                    const string& token, int currentPC,
                    bool asOffset16, bool isBranch, int& outVal){
    long long val=0;
    string t = rtrim(token);

    if (looksNumber(t)){
        // เป็นตัวเลขตรง ๆ → แปลงเป็น long long
        ErrInfo e = parseNumber(t,val);
        if(e.code!=AsmError::NONE) return e;
    }else{
        // เป็น label → หา address จาก symbol table
        int addr=0; ErrInfo e = findLabel(symtab, t, addr);
        if (e.code!=AsmError::NONE) return e;
        // ถ้าเป็น beq: ใช้ offset แบบ relative เป้าหมาย - (PC+1)
        if (isBranch) val = (long long)addr - (long long)(currentPC+1);
        else          val = addr;
    }

    // ถ้าเป็น offset 16 บิต ต้องไม่เกินช่วง signed 16-bit
    if (asOffset16 && !inSigned16(val))
        return {AsmError::OFFSET_OUT_OF_RANGE, "offset out of 16-bit range: " + to_string(val)};

    outVal = (int)val;
    return {AsmError::NONE,""};
}

// -------------------- โครงสร้าง IR (รับจาก Part A) --------------------
// หมายเหตุ: Part A จะ parse ข้อความ assembly แล้วส่ง IR ให้เรา (Part B)
// - mnemonic: ชื่อคำสั่ง เช่น add/lw/beq/.../.fill
// - regA/regB/dest: ตำแหน่งเรจิสเตอร์ที่เกี่ยวข้อง (R/J type ใช้ dest)
// - fieldToken: ค่าฟิลด์ (offset/label) สำหรับ I-type และค่าใน .fill
// - pc: address ของคำสั่งบรรทัดนั้น (เริ่มนับจาก 0)
struct IRInstr {
    string mnemonic;
    int    regA{-1}, regB{-1}, dest{-1};
    string fieldToken;
    int    pc{-1};
};

// -------------------- helper: ตรวจ register ให้ครบและอยู่ในช่วง --------------------
static ErrInfo needReg(int reg, const string& name){
    if (reg < 0)
        return {AsmError::BAD_REGISTER, "missing "+name}; // ไม่ได้ใส่มา
    if (!okReg(reg))
        return {AsmError::BAD_REGISTER, "bad "+name+": "+to_string(reg)}; // เกินช่วง 0..7
    return {AsmError::NONE,""};
}

// -------------------- เข้ารหัสคำสั่งเดี่ยว (Week 3) --------------------
// ผลลัพธ์: word = machine code (32-bit) ในรูป int32_t สำหรับพิมพ์เป็นฐาน 10
struct EncodeResult {
    ErrInfo  error;
    int32_t  word{0};
};

EncodeResult assembleOne(const unordered_map<string,int>& symtab, const IRInstr& ir){
    EncodeResult r;
    int opcode=-1;

    // 1) แปลง mnemonic -> opcode (ถ้า .fill จะให้ opcode=-1)
    r.error = toOpcode(ir.mnemonic, opcode);
    if (r.error.code!=AsmError::NONE) return r;

    // 2) .fill: ไม่ใช่คำสั่ง ให้คืนค่าตัวเลข/addr ตรง ๆ
    if (opcode < 0){
        int val=0;
        ErrInfo e = getFieldValue(symtab, ir.fieldToken, ir.pc, false, false, val);
        if (e.code!=AsmError::NONE){ r.error=e; return r; }
        r.word = val;
        return r;
    }

    // 3) เลือก pack ตามชนิดของ opcode (R/I/J/O)
    switch (static_cast<Op>(opcode)){
        case Op::ADD:
        case Op::NAND: {
            // R-type: add/nand regA regB dest
            ErrInfo e = needReg(ir.regA, "regA"); if (e.code!=AsmError::NONE){ r.error=e; return r; }
            e = needReg(ir.regB, "regB");         if (e.code!=AsmError::NONE){ r.error=e; return r; }
            e = needReg(ir.dest, "destReg");      if (e.code!=AsmError::NONE){ r.error=e; return r; }
            r.word = (int32_t)packR(opcode, ir.regA, ir.regB, ir.dest);
            return r;
        }
        case Op::LW:
        case Op::SW: {
            // I-type: lw/sw regA regB offset(16 บิต signed)
            ErrInfo e = needReg(ir.regA, "regA"); if (e.code!=AsmError::NONE){ r.error=e; return r; }
            e = needReg(ir.regB, "regB");         if (e.code!=AsmError::NONE){ r.error=e; return r; }
            int off=0;
            e = getFieldValue(symtab, ir.fieldToken, ir.pc, true, false, off);
            if (e.code!=AsmError::NONE){ r.error=e; return r; }
            r.word = (int32_t)packI(opcode, ir.regA, ir.regB, off);
            return r;
        }
        case Op::BEQ: {
            // I-type: beq regA regB offset(label) → relative = labelAddr - (PC+1)
            ErrInfo e = needReg(ir.regA, "regA"); if (e.code!=AsmError::NONE){ r.error=e; return r; }
            e = needReg(ir.regB, "regB");         if (e.code!=AsmError::NONE){ r.error=e; return r; }
            int off=0;
            e = getFieldValue(symtab, ir.fieldToken, ir.pc, true, true, off);
            if (e.code!=AsmError::NONE){ r.error=e; return r; }
            r.word = (int32_t)packI(opcode, ir.regA, ir.regB, off);
            return r;
        }
        case Op::JALR: {
            // J-type: jalr regA regB (ไม่ใช้ช่อง 16 บิตท้าย)
            ErrInfo e = needReg(ir.regA, "regA"); if (e.code!=AsmError::NONE){ r.error=e; return r; }
            e = needReg(ir.regB, "regB");         if (e.code!=AsmError::NONE){ r.error=e; return r; }
            r.word = (int32_t)packJ(opcode, ir.regA, ir.regB);
            return r;
        }
        case Op::HALT:
        case Op::NOOP: {
            // O-type: เฉพาะ opcode ที่เหลือเป็น 0
            r.word = (int32_t)packO(opcode);
            return r;
        }
        default:
            r.error = {AsmError::UNKNOWN_OPCODE, "unhandled opcode"};
            return r;
    }
}

// -------------------- Week 4: assembleProgram (fail-fast + เขียนไฟล์) --------------------
// แนวคิด:
//   - วนทุก IR → แปลงเป็น machine code
//   - ถ้าเจอ error บรรทัดใด → รายงานและหยุดทันที (ไม่เขียนไฟล์ต่อ) → return 1
//   - ถ้าสำเร็จครบ → เขียน "เลขฐาน 10" ลงไฟล์ บรรทัดละ 1 ค่า → return 0
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
            // รายงาน PC และคำสั่งเพื่อให้ debug ตรงจุดได้ง่าย
            cerr << "ERROR at PC=" << ir.pc
                << " (" << ir.mnemonic << "): " << res.error.msg << "\n";
            return 1; // หยุดทันทีตามสเปก
        }
        // เขียนเป็นเลขฐาน 10 หนึ่งค่า/บรรทัดตามข้อกำหนด
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

// -------------------- โหลด Symbols & IR (ทนทานต่อช่องว่าง/คอลัมน์) --------------------
// ไฟล์ symbolsTable.txt คาดว่าแต่ละบรรทัดเป็น: "<label> <addr>"
// บรรทัดแรกอาจเป็น header จึงข้ามไปหนึ่งบรรทัด
unordered_map<string,int> loadSymbolTable(const string& filename){
    unordered_map<string,int> symbols;
    ifstream fin(filename);
    if(!fin){ cerr<<"ERROR: cannot open symbols file: "<<filename<<"\n"; return symbols; }

    string line;
    getline(fin, line); // ข้าม header ถ้ามี

    while (getline(fin, line)) {
        if (line.empty()) continue;
        istringstream iss(line);
        string label; int addr;
        if (iss >> label >> addr)
            symbols[label] = addr; // เก็บ label -> address
        // ถ้า parse ไม่ได้ (format เพี้ยน) เราข้ามไป เพื่อไม่ให้พังทั้งไฟล์
    }

    cerr << "Loaded " << symbols.size() << " symbol(s) from: " << filename << "\n";
    return symbols;
}

// ฟอร์แมตไฟล์ program.ir ตามคอลัมน์คงที่ (fixed width) ที่ใช้อยู่ในทีม:
// [0..7]=addr, [8..15]=label, [16..23]=instr, [24..31]=f0, [32..39]=f1, [40..47]=f2,
// [48..55]=regA, [56..63]=regB, [64..71]=dest, [72..79]=off, [80..]=fill
// หมายเหตุ:
// - เราอ่านแบบ "ตัดคอลัมน์" + rtrim ปลายทาง เพื่อเก็บ token ที่สะอาด
// - IR ที่ต้องการสำหรับ Part B คือ:
//     mnemonic, regA, regB, dest, fieldToken (กรณี I-type/.fill), pc
vector<IRInstr> loadIR(const string& filename){
    vector<IRInstr> IR;
    ifstream fin(filename);
    if(!fin){ cerr<<"ERROR: cannot open IR file: "<<filename<<"\n"; return IR; }

    string line;
    getline(fin, line); // header

    while (getline(fin, line)) {
        if (line.empty()) continue;

        IRInstr ir;

        // format ของ parser.writeIRFile():
        // addr(8) | label(8) | instr(8) | f0(8) | f1(8) | f2(10)
        // | regA(8) | regB(8) | dest(8) | offset16(10) | fillValue(10)
        string addrStr = safeSubstr(line, 0, 8);             // [0..7]
        string label   = rtrim(safeSubstr(line, 8, 8));      // [8..15]  (ไม่ใช้ก็ได้)
        string instr   = rtrim(safeSubstr(line,16, 8));      // [16..23]
        string f0      = rtrim(safeSubstr(line,24, 8));      // [24..31]
        string f1      = rtrim(safeSubstr(line,32, 8));      // [32..39]
        string f2      = rtrim(safeSubstr(line,40,10));      // [40..49]

        string regAstr = safeSubstr(line,50, 8);             // [50..57]
        string regBstr = safeSubstr(line,58, 8);             // [58..65]
        string destr   = safeSubstr(line,66, 8);             // [66..73]
        string offstr  = safeSubstr(line,74,10);             // [74..83] (อ่านได้แต่ไม่ใช้)
        string fillstr = safeSubstr(line,84,10);             // [84..93]

        // 1) addr → pc (สำคัญมากสำหรับ beq PC-relative)
        int pcTmp;
        if (!tryParseInt(addrStr, pcTmp)) {
            // ถ้า parse ไม่ได้ ข้ามแถวเงียบ ๆ หรือจะ error ก็ได้
            cerr << "WARN: bad addr field, skip line: " << line << "\n";
            continue;
        }
        ir.pc = pcTmp;

        // 2) ถ้าไม่มี mnemonic จริง ๆ ให้ข้าม (กันบรรทัดรกลน)
        if (instr.empty()) continue;

        // 3) parse reg ตัวเลข (ถ้า parse ไม่ได้ให้เป็น -1)
        int tmp;
        if (tryParseInt(regAstr, tmp)) ir.regA = tmp; else ir.regA = -1;
        if (tryParseInt(regBstr, tmp)) ir.regB = tmp; else ir.regB = -1;
        if (tryParseInt(destr,   tmp)) ir.dest = tmp; else ir.dest = -1;

        // 4) fieldToken ที่ assembler ใช้จริง
        if (instr == ".fill") {
            ir.fieldToken = !f0.empty() ? f0 : rtrim(fillstr);
        } else if (instr == "lw" || instr == "sw" || instr == "beq") {
            ir.fieldToken = f2;  // ใช้ field2 (label/ตัวเลขดิบ) ไม่ใช่ offset16 ที่คอลัมน์หลัง
        } else {
            ir.fieldToken.clear();
        }

        ir.mnemonic = instr;
        IR.push_back(ir);
    }

    cerr << "Loaded " << IR.size() << " IR row(s) from: " << filename << "\n";
    return IR;
}


// -------------------- main: ผูกทุกอย่างเข้าด้วยกัน + exit code --------------------
// การทำงานหลัก:
//   - รับพาธไฟล์จาก argv (หรือใช้ดีฟอลต์)
//   - โหลดสัญลักษณ์+IR
//   - ถ้า IR ว่าง → error ทันที
//   - เรียก assembleProgram เพื่อแปลงและเขียนไฟล์ผลลัพธ์
//   - คืนค่า 0/1 ตามผล (สอดคล้องสเปก project)
int main(int argc, char** argv){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // ดีฟอลต์ชื่อไฟล์ (สามารถส่งเองผ่าน argv)
    string irPath  = (argc >= 2 ? argv[1] : "program.ir");
    string symPath = (argc >= 3 ? argv[2] : "symbolsTable.txt");
    string outPath = (argc >= 4 ? argv[3] : "machineCode.mc");

    // โหลดข้อมูลที่จำเป็น
    auto symtab = loadSymbolTable(symPath);
    auto irs    = loadIR(irPath);

    // ถ้าไม่มี IR → ไม่มีอะไรให้แปลง → นับเป็น error
    if (irs.empty()) {
        cerr << "ERROR: no IR to assemble (check " << irPath << ")\n";
        return 1;
    }

    cerr << "--- Assembling " << irs.size() << " instruction(s) ---\n";

    // วนประกอบ + เขียนไฟล์ (fail-fast)
    int code = assembleProgram(symtab, irs, outPath);

    if (code == 0) {
        cout << "Assemble success. Wrote machine code to: " << outPath << "\n";
    } else {
        cerr << "Assemble failed. See errors above.\n";
    }
    return code; // 0=สำเร็จ, 1=ล้มเหลว (สอดคล้องกับข้อกำหนด)
}
