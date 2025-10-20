// assembler.cpp
/*ภาพรวมการทำงาน (Overall)
โค้ดนี้คือ “ตัวแอสเซมเบลอร์ส่วน B” ที่รับ Symbol Table กับ IR ที่ parse แล้ว (มาจากส่วน A) แล้ว
เดินทีละคำสั่ง (IR row)
แปลงเป็นคำสั่งเครื่อง 32 บิตตามรูปแบบบิตที่กำหนด (R/I/J/O)
ตรวจ error ต่าง ๆ (label ไม่มี, offset เกิน 16 บิต, เรจิสเตอร์ผิดช่วง ฯลฯ)
เขียนผลเป็น เลขฐานสิบบรรทัดละ 1 ค่า ลงไฟล์ .mc และพิมพ์ log (ทั้งฐานสิบและ hex) ออกทาง stdout/stderr*/

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
#include <bitset>  // ใช้พิมพ์เลขฐานสอง 3 บิตของ opcode (เช่น 000..111)


using namespace std;

// ---------- ยูทิลพื้นฐานเล็ก ๆ (คุณภาพชีวิต) ----------

// rtrim: ตัดช่องว่าง(space/tab/newline)ทางขวาสุดของสตริง (กันปัญหาช่องว่างปนจากไฟล์คอลัมน์คงที่)
// ใช้ find_if จากท้ายไปหน้าเพื่อหาตำแหน่งตัวอักษรตัวสุดท้ายที่ “ไม่ใช่ช่องว่าง” แล้ว erase ส่วนเกิน
static inline string rtrim(string s) {
    auto it = find_if(s.rbegin(), s.rend(), [](unsigned char ch){return !isspace(ch);});
    s.erase(it.base(), s.end());
    return s;
}

// safeSubstr: substr แบบปลอดภัย (ถ้า pos เกินความยาว ให้คืน "" เพื่อกัน out_of_range)
//ใช้ตอน “ตัดคอลัมน์คงที่” จากไฟล์ IR
static inline string safeSubstr(const string& s, size_t pos, size_t len) {
    if (pos >= s.size()) return "";
    len = min(len, s.size() - pos);
    return s.substr(pos, len);
}

// tryParseInt: แปลงสตริงเป็น int (รองรับฐาน 10/16 ด้วย stoll base 0 (แบบ auto) ) 
// ถ้าพังให้ false (ถ้ามีขยะต่อท้าย, เกินช่วง int32, หรือแปลงไม่ได้)
// ใช้ตอนอ่านคอลัมน์เลข (เช่น regA/regB/dest) ในไฟล์ IR
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
/* กำหนดรหัสคำสั่ง add=0, nand=1, lw=2, sw=3, beq=4, jalr=5, halt=6, noop=7
.fill ไม่ใช่ instruction → ใช้ค่า -1 เพื่อบอกว่าเป็น directive
OPCODE_MAP ช่วยแปลง mnemonic (สตริง) → Op ได้รวดเร็ว*/
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
constexpr int OPCODE_SHIFT = 22; //OPCODE_SHIFT=22
constexpr int REGA_SHIFT   = 19; //REGA_SHIFT=19
constexpr int REGB_SHIFT   = 16; //REGB_SHIFT=16
//opcode อยู่บิต [24..22] (3 บิต)
//regA บิต [21..19] (3 บิต)
//regB บิต [18..16] (3 บิต)
//แล้วที่เหลือขึ้นกับชนิดคำสั่ง

// ฟังก์ชัน pack บิตสำหรับแต่ละฟอร์แมต (ลด duplicate โค้ด)
inline uint32_t packR(int opcode, int rA, int rB, int dest) {
    // R-type(add, nand) : ช่องท้ายสุดใช้เพียง 3 บิตสำหรับ destReg (3 บิตที่ [2..0]) ส่วน [15..3] เป็น 0)
    return (uint32_t(opcode) << OPCODE_SHIFT)
        | (uint32_t(rA)     << REGA_SHIFT)
        | (uint32_t(rB)     << REGB_SHIFT)
        | (uint32_t(dest) & 0x7u); //dest & 0x7u บังคับ 3 บิต
}
inline uint32_t packI(int opcode, int rA, int rB, int offset16) {
    // I-type(lw, sw, beq): 16 บิตท้ายใช้เก็บ offset แบบ two's complement
    return (uint32_t(opcode) << OPCODE_SHIFT)
        | (uint32_t(rA)     << REGA_SHIFT)
        | (uint32_t(rB)     << REGB_SHIFT)
        | (uint32_t(offset16) & 0xFFFFu); // & 0xFFFFu เพื่อชัดเจนว่าตัดเหลือ 16 บิต
}
inline uint32_t packJ(int opcode, int rA, int rB) {
    // J-type(jalr): ใช้แค่ opcode + regA + regB, ที่เหลือ (16 บิต[15..0]) เป็น 0
    return (uint32_t(opcode) << OPCODE_SHIFT)
        | (uint32_t(rA)     << REGA_SHIFT)
        | (uint32_t(rB)     << REGB_SHIFT);
}
inline uint32_t packO(int opcode) {
    // ใช้กับ halt, noop
    // O-type: ใช้เฉพาะ opcode, ที่เหลือทั้งหมดเป็น 0
    return (uint32_t(opcode) << OPCODE_SHIFT);
}

// -------------------- โครงสร้าง error ที่เราจะรายงาน --------------------
enum class AsmError {
    NONE = 0,
    UNKNOWN_OPCODE,   // คำสั่งไม่อยู่ใน mapping ;opcode ไม่รู้จัก
    UNDEFINED_LABEL,  // อ้าง label ที่ไม่มีใน symbol table ; label ไม่มี,
    OFFSET_OUT_OF_RANGE, // offset 16-bit เกินช่วง signed (-32768..32767) ; offset เกิน 16 บิต
    BAD_IMMEDIATE,    // immediate/number แปลงไม่ได้หรือรูปแบบผิด ; immediate ผิด
    BAD_REGISTER      // เรจิสเตอร์หายไป หรืออยู่นอกช่วง 0..7 (3 บิต) ; register ผิดช่วง
};
struct ErrInfo {
    AsmError code{AsmError::NONE};
    string   msg;
}; // ErrInfo เก็บโค้ดและข้อความ error ไว้สื่อสารย้อนกลับ

inline bool okReg(int r){ return 0<=r && r<=7; } // reg 3 บิต: 0..7 เท่านั้น

// -------------------- Helper: mapping/parse/symbol/offset --------------------

// แปลง mnemonic → opcode (int) ด้วย OPCODE_MAP
// .fill จะให้ outOpcode = -1 
ErrInfo toOpcode(const string& mnemonic, int& outOpcode) {
    auto it = OPCODE_MAP.find(mnemonic);
    if (it == OPCODE_MAP.end())
        return {AsmError::UNKNOWN_OPCODE, "unknown opcode: " + mnemonic};
    if (it->second == Op::FILL) { outOpcode = -1; return {AsmError::NONE,""}; }
    outOpcode = static_cast<int>(it->second);
    return {AsmError::NONE,""};
}

// -----------------------------------------------------------------------------
// helper: พิมพ์ opcode จากชื่อคำสั่ง (mnemonic) แบบ CLI
// การใช้งาน: printOpcodeCLI("add")  → พิมพ์ "add -> opcode 0 (bin 000)"
// หมายเหตุ:
//   - ใช้ฟังก์ชันเดิมของโปรเจกต์: toOpcode(mnemonic, outOpcode)
//   - ถ้าเป็น ".fill" จะถือเป็น directive (ไม่ใช่ instruction) → opcode = -1
// -----------------------------------------------------------------------------
static int printOpcodeCLI(const std::string& m) {
    int opcode = -1;  // ค่าตั้งต้น (-1) เผื่อกรณีไม่ใช่ instruction เช่น .fill

    // เรียก mapping ชื่อคำสั่ง → ตัวเลข opcode
    //   - สำเร็จ: opcode จะเป็น 0..7 (add..noop) หรือ -1 ถ้าเป็น .fill
    //   - ล้มเหลว: code != NONE แปลว่าไม่รู้จัก mnemonic นี้
    ErrInfo e = toOpcode(m, opcode);

    // ถ้าไม่รู้จัก mnemonic → แจ้ง error และจบด้วยรหัส 1 (ผิดพลาด)
    if (e.code != AsmError::NONE) {
        std::cerr << "unknown mnemonic: " << m << "\n";
        return 1;
    }

    // พิมพ์ชื่อคำสั่งและเลข opcode (แบบฐานสิบ) ออกไปก่อน
    std::cout << m << " -> opcode " << opcode;

    // ถ้าเป็นคำสั่งจริง (opcode 0..7) แถมรูปแบบฐานสอง 3 บิตให้ดูด้วย
    // ตัวอย่าง: 4 → "100" (beq)
    if (opcode >= 0) {
        std::cout << " (bin " << std::bitset<3>(opcode) << ")";
    } else {
        // กรณีพิเศษ: .fill ไม่ใช่ instruction → แสดงว่าเป็น directive ชัดเจน
        std::cout << " (directive)"; // .fill = -1
    }

    // ปิดท้ายด้วยขึ้นบรรทัดใหม่ แล้วคืนค่า 0 (สำเร็จ)
    std::cout << "\n";
    return 0;
}


// helper เช็คว่า x อยู่ในช่วง signed 16-bit หรือไม่
inline bool inSigned16(long long x){ return -32768<=x && x<=32767; }

// ตรวจ string ว่าเป็น “เลข” มั้ย (รองรับ +/-, 0x..)
bool looksNumber(const string& s){
    if (s.empty()) return false;
    size_t i=0; if (s[0]=='+'||s[0]=='-') i=1;
    if (i>=s.size()) return false;

    // รูปแบบ 0x.. (hex) ฐาน 16
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
// parseNumber ใช้ stoll base 0 และกันขยะต่อท้าย
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
// มองหา label ใน unordered_map ถ้าไม่เจอ → error UNDEFINED_LABEL
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
        // เป็น label → หา address จาก symbol table(symtab)
        int addr=0; ErrInfo e = findLabel(symtab, t, addr);
        if (e.code!=AsmError::NONE) return e;

        // ถ้า isBranch=true (beq) → แปลงเป็น relative offset = labelAddr - (PC+1)**
        if (isBranch) val = (long long)addr - (long long)(currentPC+1);
        // ถ้า isBranch=false (lw/sw/.fill) → ใช้ address ตรง ๆ (word address)**
        else          val = addr;
    }

    // ถ้า asOffset16=true → บังคับค่าที่ได้ต้องอยู่ในช่วง signed 16-bit (-32768..32767)
    // ไม่งั้น error OFFSET_OUT_OF_RANGE
    if (asOffset16 && !inSigned16(val))
        return {AsmError::OFFSET_OUT_OF_RANGE, "offset out of 16-bit range: " + to_string(val)};

    outVal = (int)val;
    return {AsmError::NONE,""};
}

// -------------------- โครงสร้าง IR (รับจาก Part A) --------------------
// หมายเหตุ: Part A(ปฟ) จะ parse ข้อความ assembly แล้วส่ง IR ให้เรา (Part B (นน)) 
// - mnemonic: ชื่อคำสั่ง เช่น add/lw/beq/.../.fill
// - regA/regB/dest: ตำแหน่งเรจิสเตอร์ที่เกี่ยวข้อง (R/J type ใช้ dest)
// - fieldToken: ค่าฟิลด์ (offset/label) สำหรับ I-type และค่าใน .fill
// - pc: address ของคำสั่งบรรทัดนั้น (เริ่มนับจาก 0)
struct IRInstr {
    string mnemonic; //mnemonic: เช่น add, lw, .fill
    int    regA{-1}, regB{-1}, dest{-1}; // regA, regB, dest: เลขเรจิสเตอร์ (ถ้าไม่ระบุจะเป็น -1)
    string fieldToken; // fieldToken: token ของฟิลด์ท้าย (offset/label สำหรับ lw/sw/beq หรือค่าของ .fill)
    int    pc{-1}; // pc: address ของบรรทัดนี้ (เริ่มที่ 0)
};

// -------------------- helper: ตรวจ register ให้ครบและอยู่ในช่วง --------------------
// needReg ตรวจว่ามีค่าไหม (ไม่ใช่ -1) และอยู่ในช่วงหรือไม่
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
    // assembler แสดง opcode ของทุกบรรทัด
    cout << "Mnemonic " << ir.mnemonic << " => Opcode " << opcode << endl;

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

        // 1) อ่านฟิลด์พื้นฐานจากคอลัมน์คงที่
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
        string fillstr = safeSubstr(line,80, 16); // กันอนาคตถ้าฟิลด์ยาว

        // 2) แปลงค่าที่เป็นตัวเลข ถ้าแปลงไม่ได้ให้เป็น -1 (หมายถึง "ไม่กรอก")
        int tmp;
        if (tryParseInt(regAstr, tmp)) ir.regA = tmp; else ir.regA = -1;
        if (tryParseInt(regBstr, tmp)) ir.regB = tmp; else ir.regB = -1;
        if (tryParseInt(destr,   tmp)) ir.dest = tmp; else ir.dest = -1;

        // 3) ตัดสินใจ fieldToken ที่ Part B ต้องใช้:
        //    - .fill → ใช้ f0 เป็นหลัก (ตามฟอร์แมตที่วาง), ถ้าไม่มีค่อยลองดู fillstr
        //    - lw/sw/beq → ใช้ f2 (offset/label)
        //    - อื่น ๆ → เว้นว่าง
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

    // ---------------- เช็ค opcode ผ่าน CLI ----------- -----
    // รูปแบบการเรียก:
    //   assembler.exe --opcode <mnemonic>
    // ตัวอย่าง:
    //   assembler.exe --opcode add
    //   assembler.exe --opcode beq
    //   assembler.exe --opcode ".fill"
    //
    // ข้อดี: ไม่ไปแตะไฟล์ IR/Symbols เลย เอาไว้เช็ค mapping เร็ว ๆ
    if (argc >= 2 && std::string(argv[1]) == "--opcode") {
        // ต้องมีอาร์กิวเมนต์ตัวที่ 2 เป็นชื่อคำสั่ง
        if (argc < 3) {
            std::cerr << "usage: " << argv[0] << " --opcode <mnemonic>\n"
                    << "ex:    " << argv[0] << " --opcode add\n";
            return 1;
        }
        // เรียก helper แล้วจบโปรแกรมในโหมดนี้ทันที (ไม่ไปทำงาน assemble ปกติ)
        return printOpcodeCLI(argv[2]);
    }


    // ดีฟอลต์ชื่อไฟล์ (สามารถส่งเองผ่าน argv)
    string irPath  = (argc >= 2 ? argv[1] : "program.ir");
    string symPath = (argc >= 3 ? argv[2] : "program_symbols.txt");
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
