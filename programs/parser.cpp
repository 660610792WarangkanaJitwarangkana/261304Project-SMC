// Compile : g++ -std=c++17 parser.cpp -o parser 
// Run : .\parser

#include "parser.h"
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <cctype>
#include <stdexcept>
#include <iostream>
#include <limits>
#include <iomanip>

using namespace std;

// set ของ mnemonic ที่เก็บ opcode(ชื่อคำสั่ง) ที่ valid
static const unordered_set<string> MNEMONICS = {
    "add","nand","lw","sw","beq","jalr","halt","noop", ".fill"
};

// เช็คว่าค่าที่รับเข้ามาเป็นตัวเลขมั้ย
bool isNumber(const string &s) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '+' || s[0] == '-') i = 1;  // รองรับทั้ง + และะ -
    if (i == s.size()) return false;                
    for (; i < s.size(); ++i) 
        if (!isdigit((unsigned char)s[i])) return false;
    return true;
}

// เช็คว่ารูปแบบของ label ถูกต้องตามเงื่อนไขมั้ย
static bool validLabelName(const string &s) {
    if (s.empty()) return false;
    if (!isalpha((unsigned char)s[0])) return false;    // ต้องขึ้นต้นด้วยตัวอักษร
    if (s.size() > 6) return false;                     // ความยาวไม่เกิน 6 ตัวอักษร
    for (char c: s)                                     
        if (!isalnum((unsigned char)c)) return false;   // ตัวอักษรที่เหลือเป็นตัวอักษรหรือตัวเลขก็ได้
    return true;
}

// ถ้ามี error ให้แสดงใน terminal แล้ว exit
void dieError(const string &msg) {
    cerr << "Error: " << msg << "\n";
    exit(1);
}

// แยก string เป็น tokens ตาม whitespace
static vector<string> tokenize_ws(const string &s) {
    vector<string> toks;
    istringstream iss(s);
    string t;
    while (iss >> t) toks.push_back(t);
    return toks;
}


Parser::Parser() {}

// อ่านไฟล์ทั้งหมดและตัด comment ออก
void Parser::readAllLines(const string &filename, const string &commentChars) {
    rawLines.clear();
    ifstream ifs(filename);
    if (!ifs.is_open()) throw runtime_error("cannot open input file: " + filename);
    string line;
    while (getline(ifs, line)) {
        
        // ตัด comment ออกถ้ามี commentChars อยู่
        if (!commentChars.empty()) {
            size_t pos = string::npos;
            for (char cc : commentChars) {
                size_t p = line.find(cc);
                if (p != string::npos) {
                    if (pos == string::npos) pos = p;
                    else pos = min(pos, p);
                }
            }
            if (pos != string::npos) line = line.substr(0, pos);
        }

        // เก็บ raw line (อาจเป็น blank line หรือ whitespace)
        rawLines.push_back(line);
    }
    ifs.close();
}

// pass1: สร้าง symbol table และ IR preliminary
void Parser::pass1_buildSymbolTable(bool countBlankLines) {
    ir.clear();
    symbols.clear();
    unordered_map<string,int> labelToAddr;
    int addr = 0;

    for (size_t lineno = 0; lineno < rawLines.size(); ++lineno) {
        string line = rawLines[lineno];
        vector<string> toks = tokenize_ws(line);
        bool isBlank = toks.empty();
        IRLine L;
        L.address = addr;

        // เป็นบรรทัดว่าง
        if (isBlank) {  
            if (countBlankLines) {  
                // จะนับช่องว่างก็ต่อเมื่อคำสั่งเป็น noop และไม่มี label
                L.instr = "noop";
                L.rawLabel = "";
            } else {
                // ถ้าไม่มีคำสั่งหรืออะไรในบรรทัดเลยก็ข้ามบรรทัดนี้ไปเลย ไม่ต้องเก็บ address
                continue;
            }

        // ไม่ใช่บรรทัดว่าง    
        } else {        
            // เช็คว่า tokens ที่เก็บมาตัวแรกเป็น label หรือ mnemonic
            string first = toks[0];
            bool firstIsMnemonic = (MNEMONICS.find(first) != MNEMONICS.end());
            
            // ถ้าตัวแรกเป็น label ไม่ใช่ mnemonic
            if (!firstIsMnemonic) {   

                // เช็ค validity ของ label ว่าถูกต้องมั้ย
                if (!validLabelName(first)) {
                    throw runtime_error("invalid label name '" + first + "' at source line " + to_string(lineno+1));
                }

                // เช็คว่ามี label ซ้ำรึเปล่า
                if (labelToAddr.find(first) != labelToAddr.end()) {
                    throw runtime_error("duplicate label '" + first + "' at source line " + to_string(lineno+1));
                }

                labelToAddr[first] = addr;
                symbols.push_back({first, addr});
                L.rawLabel = first;

                // ถ้ามี instruction ตามหลัง label
                if (toks.size() >= 2) L.instr = toks[1];
                if (toks.size() >= 3) L.f0 = toks[2];
                if (toks.size() >= 4) L.f1 = toks[3];
                if (toks.size() >= 5) L.f2 = toks[4];

            // ไม่มี label ตัวแรกเป็น mnemonic เลย    
            } else {  
                L.rawLabel = "";
                L.instr = first;
                if (toks.size() >= 2) L.f0 = toks[1];
                if (toks.size() >= 3) L.f1 = toks[2];
                if (toks.size() >= 4) L.f2 = toks[3];
            }
        }

        ir.push_back(L);
        addr++;
    }
}

// pass2: resolve operand, registers, offsets
void Parser::pass2_resolve(bool countBlankLines) {

    // สร้าง map ของ label กับ address เพื่อ lookup เร็วขึ้น
    unordered_map<string,int> labelToAddr;
    for (const auto &lab : symbols) labelToAddr[lab.name] = lab.address;

    for (size_t i = 0; i < ir.size(); ++i) {
        IRLine &L = ir[i];
        string m = L.instr;

        // ถ้าช่องคำสั่งว่างมีแค่ label แต่ไม่มีคำสั่ง (ไม่ควรเกิดขึ้น) 
        if (m.empty()) {
            throw runtime_error("missing instruction at address " + to_string(L.address));
        }

        // เช็คว่า opcode(คำสั่ง) ถูกต้องมั้ย
        if (MNEMONICS.find(m) == MNEMONICS.end()) {
            throw runtime_error("invalid opcode '" + m + "' at address " + to_string(L.address));
        }

        // แยกข้อมูลตามชนิดคำสั่ง
        // .fill ใช้ใส่ค่าตัวเลขหรือตำแหน่ง label ลงใน memory
        if (m == ".fill") {
            L.isFill = true;
            // ไม่มี field1 (f0) ตามหลัง
            if (L.f0.empty()) 
                throw runtime_error(".fill without operand at address " + to_string(L.address));
            // ถ้าเป็นตัวเลข เก็บเลขนั้นลงใน mem
            if (isNumber(L.f0)) {   
                long long v = stoll(L.f0);
                L.fillValue = static_cast<int>(v);
            // ถ้าเป็นชื่อ label ให้หา address ของ label น้้นๆ
            } else {
                // ถ้าไม่มีใน list ของ label ที่เคยเก็บแสดงว่า error
                if (labelToAddr.find(L.f0) == labelToAddr.end()) 
                    throw runtime_error("undefined label '" + L.f0 + "' used in .fill at address " + to_string(L.address));
                L.fillValue = labelToAddr[L.f0];
            }
            continue;
        }

        // R-type: add, nand
        if (m == "add" || m == "nand") {
            // ไล่เช็คว่ามีครบทุก field มั้ย
            if (L.f0.empty() || L.f1.empty() || L.f2.empty()) 
                throw runtime_error("R-type instruction missing field at address " + to_string(L.address));
            // เช็คว่าทุก field เป็นเลขมั้ย
            if (!isNumber(L.f0) || !isNumber(L.f1) || !isNumber(L.f2)) 
                throw runtime_error("R-type registers must be numeric at address " + to_string(L.address));
            // แปลงเป็น int แล้วเช็ค reg ว่าอยู่ในช่วง 0–7 มั้ย
            L.regA = stoi(L.f0);
            L.regB = stoi(L.f1);
            L.dest = stoi(L.f2);
            if (L.regA < 0 || L.regA > 7 || L.regB < 0 || L.regB > 7 || L.dest < 0 || L.dest > 7)
                throw runtime_error("register out of range (0..7) at address " + to_string(L.address));
            continue;
        }

        // I-type: lw, sw
        if (m == "lw" || m == "sw") {
            // ไล่เช็คว่ามีครบทุก field มั้ย
            if (L.f0.empty() || L.f1.empty() || L.f2.empty()) 
                throw runtime_error("lw/sw missing field at address " + to_string(L.address));
            // เช็คว่า regA, regB เป็นตัวเลขมั้ย
            if (!isNumber(L.f0) || !isNumber(L.f1)) 
                throw runtime_error("lw/sw regA/regB must be numeric at address " + to_string(L.address));
            // แปลงเป็น int แล้วเช็ค reg ว่าอยู่ในช่วง 0–7 มั้ย    
            L.regA = stoi(L.f0);
            L.regB = stoi(L.f1);
            if (L.regA < 0 || L.regA > 7 || L.regB < 0 || L.regB > 7) 
                throw runtime_error("register out of range (0..7) at address " + to_string(L.address));
            // ถ้า offset เป็นตัวเลข ให้แปลงค่าแล้วเช็คว่าอยู่ในช่วง 16 บิต
            if (isNumber(L.f2)) {
                long long v = stoll(L.f2);
                if (v < -32768 || v > 32767) 
                    throw runtime_error("offset out of 16-bit range for lw/sw at address " + to_string(L.address));
                L.offset16 = static_cast<int>(v);
            // ถ้า offset เป็น label ให้แทน label ด้วย addr แล้วเช็คว่า addr อยู่ในช่วง 16 บิต มั้ย้
            } else {
                if (labelToAddr.find(L.f2) == labelToAddr.end()) 
                    throw runtime_error("undefined label '" + L.f2 + "' used in lw/sw at address " + to_string(L.address));
                int addrLabel = labelToAddr[L.f2];
                if (addrLabel < -32768 || addrLabel > 32767) 
                    throw runtime_error("label address out of 16-bit range for lw/sw at address " + to_string(L.address));
                L.offset16 = addrLabel;
            }
            continue;
        }

        // Branch: beq
        if (m == "beq") {
            // ไล่เช็คความถูกต้องเหมือน I-type
            if (L.f0.empty() || L.f1.empty() || L.f2.empty()) 
                throw runtime_error("beq missing field at address " + to_string(L.address));
            if (!isNumber(L.f0) || !isNumber(L.f1)) throw runtime_error("beq regA/regB must be numeric at address " + to_string(L.address));
            L.regA = stoi(L.f0);
            L.regB = stoi(L.f1);
            if (L.regA < 0 || L.regA > 7 || L.regB < 0 || L.regB > 7) 
                throw runtime_error("register out of range (0..7) at address " + to_string(L.address));
            if (isNumber(L.f2)) {
                long long v = stoll(L.f2);
                if (v < -32768 || v > 32767) 
                    throw runtime_error("beq numeric offset out of 16-bit range at address " + to_string(L.address));
                L.offset16 = static_cast<int>(v);
            // ถ้า offset เป็น label ให้คำนวณ offset แบบ relative คือ address(label) - (address(ปัจจุบัน) + 1)
            } else {
                if (labelToAddr.find(L.f2) == labelToAddr.end()) 
                    throw runtime_error("undefined label '" + L.f2 + "' used in beq at address " + to_string(L.address));
                int addrLabel = labelToAddr[L.f2];
                long long offset = static_cast<long long>(addrLabel) - (static_cast<long long>(L.address) + 1LL);
                if (offset < -32768 || offset > 32767) 
                    throw runtime_error("beq offset out of range for label '" + L.f2 + "' at address " + to_string(L.address));
                L.offset16 = static_cast<int>(offset);
            }
            continue;
        }

        // J-type: jalr
        if (m == "jalr") {
            // เช็ค field regA regB เหมือน I-type
            if (L.f0.empty() || L.f1.empty()) 
                throw runtime_error("jalr missing field at address " + to_string(L.address));
            if (!isNumber(L.f0) || !isNumber(L.f1)) 
                throw runtime_error("jalr registers must be numeric at address " + to_string(L.address));
            L.regA = stoi(L.f0);
            L.regB = stoi(L.f1);
            if (L.regA < 0 || L.regA > 7 || L.regB < 0 || L.regB > 7) 
                throw runtime_error("register out of range (0..7) at address " + to_string(L.address));
            continue;
        }

        // O-type: halt, noop ไม่มี field
        if (m == "halt" || m == "noop") {
            continue;
        }

        // instruction ที่ไม่รู้จัก
        throw runtime_error("unhandled instruction '" + m + "' at address " + to_string(L.address));
    }
}

void Parser::parseFile(const string &filename, bool countBlankLines, const string &commentChars) {
    readAllLines(filename, commentChars);
    pass1_buildSymbolTable(countBlankLines);
    pass2_resolve(countBlankLines);
}

const vector<IRLine>& Parser::getIR() const { return ir; }
const vector<Label>& Parser::getSymbols() const { return symbols; }

void Parser::writeIRFile(const string &outname) const {
    ofstream ofs(outname);
    if (!ofs.is_open()) throw runtime_error("cannot write IR file: " + outname);

    ofs << left
        << setw(8)  << "addr"
        << setw(8)  << "label"
        << setw(8)  << "instr"
        << setw(8)  << "field0"
        << setw(8)  << "field1"
        << setw(10) << "field2"
        << setw(8)  << "regA"
        << setw(8)  << "regB"
        << setw(8)  << "dest"
        << setw(10) << "offset16"
        << setw(10) << "fillValue"
        << "\n";

    for (const auto &L : ir) {
        ofs << setw(8)  << L.address
            << setw(8)  << L.rawLabel
            << setw(8)  << L.instr
            << setw(8)  << L.f0
            << setw(8)  << L.f1
            << setw(10) << L.f2
            << setw(8)  << L.regA
            << setw(8)  << L.regB
            << setw(8)  << L.dest
            << setw(10) << L.offset16
            << setw(10) << L.fillValue
            << "\n";
    }
    ofs.close();

}

void Parser::writeSymbolsFile(const string &outname) const {
    ofstream ofs(outname);
    if (!ofs.is_open()) throw runtime_error("cannot write symbols file: " + outname);
    
    ofs << left
        << setw(10) << "LabelName"
        << setw(10) << "Address"
        << "\n";

    for (const auto &p : symbols) {
        ofs << setw(10) << p.name
            << setw(10) << p.address
            << "\n";
    }
    ofs.close();
}

int main() {
    string inputFile = "multiply.asm";   // ไฟล์ assembly สำหรับเทส

    Parser parser;                   
    parser.parseFile(inputFile);                        // เรียกฟังก์ชันหลักเพื่ออ่านและแยกข้อมูล

    cout << "\nParsing...";
    cout << "\n-------------------------------------\n";

    cout << "Symbol Table:\n";
    for (auto &sym : parser.getSymbols()) {
        cout << "  " << setw(10) << left << sym.name
             << "-> Address: " << sym.address << endl;
    }

    cout << "\nParsed Instructions:\n";
    auto insts = parser.getIR();
    for (auto &inst : insts) {
        cout << "  " << "Address " << setw(3) << inst.address << " | "
             << setw(6) << left << inst.rawLabel << " | "
             << setw(6) << left << inst.instr << " | "
             << setw(6) << left << inst.f0 << " | "
             << setw(6) << left << inst.f1 << " | "
             << setw(6) << left << inst.f2
             << endl;
    }

    string baseName = "program";

    parser.writeIRFile(baseName + ".ir");               // สร้างไฟล์ IR สำหรับ assembler
    parser.writeSymbolsFile(baseName + "_symbols.txt"); // สร้างไฟล์ symbol table

    cout << "\n--------------------------------------------------\n";
    cout << "Parse success!\n";
    cout << "Output written to: " << baseName << ".ir and " << baseName << "_symbols.txt\n";

    return 0;
}

