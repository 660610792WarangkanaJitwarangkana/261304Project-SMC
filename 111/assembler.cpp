// ปฟ. แก้ชื่อไฟล์เป็น assembler.cpp
// Build:
//   g++ -std=c++17 assembler.cpp -o assembler.exe
//   .\assembler
// อ่านไฟล์ IR + Symbols ได้แล้ว
// รันผ่าน + ได้ machine code ที่ถูกต้องแล้ว
// ยังไม่ได้เขียนคอมเมนต์


// assembler_backend_week3_fixed.cpp
// จุดประสงค์ไฟล์:
//   - สัปดาห์ที่ 3 (Part B): เข้ารหัสคำสั่งเป็น machine code 32 บิต (R/I/J/O + .fill)
//   - ใช้ IR + symbol table (สมมติ) มาประกอบบิต ตามฟอร์แมตที่กำหนด
//   - โค้ดนี้หลีกเลี่ยง if-with-initializer เพื่อให้คอมไพล์ได้แม้ -std=c++14
//
// วิธีคิดโดยรวม:
//   1) map mnemonic -> opcode
//   2) แยกประเภทคำสั่ง (R/I/J/O/.fill) แล้ว pack บิตด้วย shift/mask
//   3) ฟังก์ชันเสริม: หา label, แปลงตัวเลข, คำนวณ offset (beq = target-(PC+1))
//   4) ตรวจ error สำคัญ: opcode ไม่รู้จัก, reg ผิดช่วง, offset เกิน 16-bit, label ไม่มี
//

#include <iostream>       // cout, cin
#include <string>         // std::string
#include <vector>         // std::vector
#include <unordered_map>  // std::unordered_map (symbol table)
#include <iomanip>        // setw (ใช้เวลา format output)
#include <cctype>         // isdigit, isxdigit
#include <stdexcept>      // สำหรับ throw exception (parse number)
#include <cstdint>        // int32_t, uint32_t
#include <fstream>
#include <sstream>

using namespace std;

// -------------------- ส่วนกำหนด Opcode --------------------
// หมายเหตุ: .fill = เคสพิเศษ (ไม่ใช่ instruction) เราจะให้เป็นค่า -1
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

// -------------------- Bit layout (การจัดบิต 32 บิต) --------------------
// รูปแบบบิต (ซ้าย->ขวา):
//   [ opcode(3) | regA(3) | regB(3) | ที่เหลือ 16 บิต/3 บิต แล้วแต่ประเภท ]
constexpr int OPCODE_SHIFT = 22;
constexpr int REGA_SHIFT   = 19;
constexpr int REGB_SHIFT   = 16;

// ฟังก์ชัน pack บิตสำหรับแต่ละฟอร์แมต
inline uint32_t packR(int opcode, int rA, int rB, int dest) {
    // R-type: opcode|regA|regB|dest(3 บิต)
    return (uint32_t(opcode) << OPCODE_SHIFT)
         | (uint32_t(rA)     << REGA_SHIFT)
         | (uint32_t(rB)     << REGB_SHIFT)
         | (uint32_t(dest) & 0x7u);
}
inline uint32_t packI(int opcode, int rA, int rB, int offset16) {
    // I-type: opcode|regA|regB|offset(16 บิต - two's complement)
    return (uint32_t(opcode) << OPCODE_SHIFT)
         | (uint32_t(rA)     << REGA_SHIFT)
         | (uint32_t(rB)     << REGB_SHIFT)
         | (uint32_t(offset16) & 0xFFFFu);
}
inline uint32_t packJ(int opcode, int rA, int rB) {
    // J-type: opcode|regA|regB|unused(16)
    return (uint32_t(opcode) << OPCODE_SHIFT)
         | (uint32_t(rA)     << REGA_SHIFT)
         | (uint32_t(rB)     << REGB_SHIFT);
}
inline uint32_t packO(int opcode) {
    // O-type: opcode|unused(22)
    return (uint32_t(opcode) << OPCODE_SHIFT);
}

// -------------------- ส่วน Error / โครงสร้างผลลัพธ์ --------------------
enum class AsmError {
    NONE = 0,
    UNKNOWN_OPCODE,     // ไม่รู้จักคำสั่ง
    UNDEFINED_LABEL,    // อ้าง label ที่ไม่มีใน symbol table
    OFFSET_OUT_OF_RANGE,// offset 16-bit เกินช่วง
    BAD_IMMEDIATE,      // immediate/number แปลงไม่ได้
    BAD_REGISTER        // หมายเลขเรจิสเตอร์ออกนอกช่วง 0..7 หรือหายไป
};

struct ErrInfo {
    AsmError code{AsmError::NONE};
    string   msg;
};

inline bool okReg(int r){ return 0<=r && r<=7; } // reg 3 บิต: 0..7

// -------------------- Helper: mapping/parse/symbol/offset --------------------

// map mnemonic -> opcode (int) (ถ้า .fill จะคืน -1)
ErrInfo toOpcode(const string& mnemonic, int& outOpcode) {
    auto it = OPCODE_MAP.find(mnemonic);
    if (it == OPCODE_MAP.end())
        return {AsmError::UNKNOWN_OPCODE, "unknown opcode: " + mnemonic};
    if (it->second == Op::FILL) { outOpcode = -1; return {AsmError::NONE,""}; }
    outOpcode = static_cast<int>(it->second);
    return {AsmError::NONE,""};
}

// เช็คช่วง signed 16-bit
inline bool inSigned16(long long x){ return -32768<=x && x<=32767; }

// ตรวจลักษณะสตริงว่าเป็นตัวเลขหรือไม่ (รองรับ +/-, dec, 0x..)
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

// แปลงสตริง -> long long (ฐานอัตโนมัติ: 0x.. = 16)
ErrInfo parseNumber(const string& token, long long& outVal){
    if (!looksNumber(token))
        return {AsmError::BAD_IMMEDIATE, "not a valid number: " + token};
    try{
        size_t pos=0;
        outVal = stoll(token, &pos, 0); // base 0 = auto (0x => hex)
        if (pos!=token.size())
            return {AsmError::BAD_IMMEDIATE, "trailing junk: " + token};
        return {AsmError::NONE,""};
    }catch(...){
        return {AsmError::BAD_IMMEDIATE, "cannot parse: " + token};
    }
}

// หา address ของ label จาก symbol table
ErrInfo findLabel(const unordered_map<string,int>& symtab,
                  const string& label, int& outAddr){
    auto it = symtab.find(label);
    if (it==symtab.end())
        return {AsmError::UNDEFINED_LABEL, "undefined label: " + label};
    outAddr = it->second;
    return {AsmError::NONE,""};
}

// คืนค่าฟิลด์ (เลข/label) โดยคำนึงถึงชนิดฟิลด์:
//   - asOffset16=true  => ต้องบีบให้เข้า 16 บิต (ถ้าเกิน => error)
//   - isBranch=true    => คำนวณ relative offset = target - (PC+1) (สำหรับ beq)
//   - isBranch=false   => ใช้ address ตรง ๆ (เช่น lw/sw/.fill เมื่อเป็น label)
ErrInfo getFieldValue(const unordered_map<string,int>& symtab,
                      const string& token, int currentPC,
                      bool asOffset16, bool isBranch, int& outVal){
    long long val=0;
    if (looksNumber(token)){
        ErrInfo e = parseNumber(token,val);
        if(e.code!=AsmError::NONE) return e;
    }else{
        int addr=0;
        ErrInfo e = findLabel(symtab, token, addr);
        if (e.code!=AsmError::NONE) return e;
        if (isBranch) val = (long long)addr - (long long)(currentPC+1);
        else          val = addr;
    }
    if (asOffset16 && !inSigned16(val))
        return {AsmError::OFFSET_OUT_OF_RANGE, "offset out of 16-bit range: " + to_string(val)};
    outVal = (int)val;
    return {AsmError::NONE,""};
}

// -------------------- โครงสร้าง IR (อินพุตจาก Part A) --------------------
// เราจะสมมติว่า Parser (Part A) แยกมาให้แบบนี้
struct IRInstr {
    string mnemonic;      // "add","lw","beq","jalr","halt","noop",".fill"
    int    regA{-1}, regB{-1}, dest{-1}; // ใช้เฉพาะบาง format (R/J ใช้ dest)
    string fieldToken;    // ใช้กับ I-type (.fill ก็ใช้: เก็บเลข/label)
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

// -------------------- สัปดาห์ที่ 3: encode เป็น machine code --------------------
struct EncodeResult {
    ErrInfo  error;
    int32_t  word{0};  // machine code base-10 (signed 32 บิตพิมพ์ออกมา)
};

EncodeResult assemble(const unordered_map<string,int>& symtab, const IRInstr& ir){
    EncodeResult r;
    int opcode=-1;

    // 1) แปลง mnemonic -> opcode (หรือ .fill = -1)
    r.error = toOpcode(ir.mnemonic, opcode);
    if (r.error.code!=AsmError::NONE) return r;

    // 2) .fill = ไม่ใช่คำสั่ง ให้คืนค่าเป็นเลข/addr ตรง ๆ
    if (opcode < 0){
        int val=0;
        ErrInfo e = getFieldValue(symtab, ir.fieldToken, ir.pc,
                                  /*asOffset16=*/false, /*isBranch=*/false, val);
        if (e.code!=AsmError::NONE){ r.error=e; return r; }
        r.word = val;
        return r;
    }

    // 3) เลือกทางตามประเภท opcode
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
            // I-type: lw/sw regA regB offset(16 บิต)
            ErrInfo e = needReg(ir.regA, "regA"); if (e.code!=AsmError::NONE){ r.error=e; return r; }
            e = needReg(ir.regB, "regB");         if (e.code!=AsmError::NONE){ r.error=e; return r; }
            int off=0;
            e = getFieldValue(symtab, ir.fieldToken, ir.pc,
                              /*asOffset16=*/true, /*isBranch=*/false, off);
            if (e.code!=AsmError::NONE){ r.error=e; return r; }
            r.word = (int32_t)packI(opcode, ir.regA, ir.regB, off);
            return r;
        }

        case Op::BEQ: {
            // I-type: beq regA regB offset(label) → relative = target - (PC+1)
            ErrInfo e = needReg(ir.regA, "regA"); if (e.code!=AsmError::NONE){ r.error=e; return r; }
            e = needReg(ir.regB, "regB");         if (e.code!=AsmError::NONE){ r.error=e; return r; }
            int off=0;
            e = getFieldValue(symtab, ir.fieldToken, ir.pc,
                              /*asOffset16=*/true, /*isBranch=*/true, off);
            if (e.code!=AsmError::NONE){ r.error=e; return r; }
            r.word = (int32_t)packI(opcode, ir.regA, ir.regB, off);
            return r;
        }

        case Op::JALR: {
            // J-type: jalr regA regB
            ErrInfo e = needReg(ir.regA, "regA"); if (e.code!=AsmError::NONE){ r.error=e; return r; }
            e = needReg(ir.regB, "regB");         if (e.code!=AsmError::NONE){ r.error=e; return r; }
            r.word = (int32_t)packJ(opcode, ir.regA, ir.regB);
            return r;
        }

        case Op::HALT:
        case Op::NOOP: {
            // O-type: ไม่มีเรจิสเตอร์/ออฟเซ็ต
            r.word = (int32_t)packO(opcode);
            return r;
        }

        default:
            r.error = {AsmError::UNKNOWN_OPCODE, "unhandled opcode"};
            return r;
    }
}

// ---------------------------------------------------------------------
// ฟังก์ชันใหม่: loadSymbolTable, loadIR, writeMachineCode 
// ปฟ.(222) แก้ไขโค้ด เพิ่มฟังก์ชันเข้ามาให้สามารถอ่านไฟล์ที่ parse มาก่อนหน้านี้ แล้วเอาไปแปลงเป็น machine code ต่อได้

unordered_map<string,int> loadSymbolTable(const string& filename){
    unordered_map<string,int> symbols;
    ifstream fin(filename);
    if(!fin){cerr<<"Cannot open symbols file: "<<filename<<"\n";return symbols;}
    
    string label; 
    int addr;
    string header; 
    getline(fin,header); // skip header line
    
    while(fin>>label>>addr) symbols[label]=addr;
    
    cout << "\nLoaded " << symbols.size() << " symbols from: " << filename << " \n " << endl;
    cout << "Symbols : \n";
    for (auto &s : symbols)
        cout << left << s.first << " = " << s.second << endl;

    return symbols;
}

vector<IRInstr> loadIR(const string& filename){
    vector<IRInstr> IR;
    ifstream fin(filename);
    if(!fin){cerr<<"Cannot open IR file: "<<filename<<"\n";return IR;}
    
    cout << "\n-------------------------------------\n";
    cout << "\nReading IR from: " << filename << endl;

    string header; 
    getline(fin,header); // skip header
    string line;

    while(getline(fin,line)){
        if(line.empty()) continue;
        IRInstr ir;
        // อ่าน addr
        string addrStr; 
        addrStr = line.substr(0,8);
        ir.pc = stoi(addrStr);

        // อ่าน label
        ir.mnemonic = ""; // default
        string label = line.substr(8,8);
        label.erase(label.find_last_not_of(" \t")+1);
        string instr = line.substr(16,8);
        instr.erase(instr.find_last_not_of(" \t")+1);
        ir.mnemonic = instr;

        // อ่าน field0/field1/field2 (รวม 24 ตัวอักษร)
        string f0 = line.substr(24,8); f0.erase(f0.find_last_not_of(" \t")+1);
        string f1 = line.substr(32,8); f1.erase(f1.find_last_not_of(" \t")+1);
        string f2 = line.substr(40,8); f2.erase(f2.find_last_not_of(" \t")+1);

        // อ่าน regA/regB/dest/offset/fill (column-based)
        int regA = stoi(line.substr(48,8));
        int regB = stoi(line.substr(56,8));
        int dest = stoi(line.substr(64,8));
        int off  = stoi(line.substr(72,8));
        int fillVal = stoi(line.substr(80));

        ir.regA = regA;
        ir.regB = regB;
        ir.dest = dest;

        // เลือก fieldToken
        if(instr==".fill") ir.fieldToken = f0;
        else if(instr=="lw"||instr=="sw"||instr=="beq") ir.fieldToken = f2;
        else ir.fieldToken = "";

        IR.push_back(ir);
    }

    cout << "\n-------------------------------------\n\n";
    
    return IR;
}

void writeMachineCode(const string& filename, const vector<int32_t>& codes) {
    ofstream fout(filename);
    if(!fout){
        cerr<<"Cannot write to "<<filename<<"\n";
        return;
    }
    for(int32_t code : codes)
        fout<<code<<"\n";
    fout.close();
    cout<<"Machine code written to : "<<filename<<"\n";
}



// -----------------------------------------------------------------
// ปฟ.(222) แก้ main ให้เรียกอ่านไฟล์ที่ได้แล้วเอาไปแปลงเป็น machine code ที่ถูกต้อง

int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    auto symtab = loadSymbolTable("symbolsTable.txt");
    auto irs = loadIR("program.ir");

    cout<<"--- Assembling "<<irs.size()<<" instructions ---\n";
    vector<int32_t> machineCodes;

    cout << "\nMachine code output:\n";
    
    for(const auto& ir:irs){
        EncodeResult res = assemble(symtab, ir);
        if(res.error.code!=AsmError::NONE){
            cout<<setw(6)<<ir.mnemonic<<" @PC="<<ir.pc<<" ERROR: "<<res.error.msg<<"\n";
        }else{
            machineCodes.push_back(res.word);
            cout << setw(3) << ir.pc << ": " << setw(12) << res.word
             << "   (0x" << hex << uppercase << res.word << dec << ")\n";
        }
    }

    cout << "\n===================================\n";

    // 🔹 Save to machineCode.mc
    writeMachineCode("machineCode.mc", machineCodes);
    cout << "Assembling completed successfully!.\n";
    cout << "===================================\n";

    return 0;

}
