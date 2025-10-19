// parser.cpp
// Compile: g++ -std=c++17 parser.cpp -o parser

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

// set ของ mnemonic(ชื่อคำสั่ง assembly)ที่ valid
static const unordered_set<string> MNEMONICS = {
    "add","nand","lw","sw","beq","jalr","halt","noop", ".fill"
};

// เช็คว่าค่าที่รับเข้ามาเป็นตัวเลขมั้ย
bool isNumber(const string &s) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '+' || s[0] == '-') i = 1;          // รองรับทั้ง + และะ -
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

        // ตัดช่องว่างหัว-ท้ายกันเคสบรรทัดเป็น "   "
        auto trim = [](string &s){
            size_t i = s.find_first_not_of(" \t\r\n");
            size_t j = s.find_last_not_of(" \t\r\n");
            if (i == string::npos) { s.clear(); return; }
            s = s.substr(i, j - i + 1);
        };
        trim(line);

        // tokenize ตาม whitespace
        vector<string> toks = tokenize_ws(line);
        bool isBlank = toks.empty();

        // ---------- บรรทัดว่าง ----------
        if (isBlank) {
            if (!countBlankLines) {
                // ไม่เก็บ IR และไม่เพิ่ม addr
                continue;
            } else {
                // ถ้านับบรรทัดว่าง → สร้าง noop 1 อัน
                IRLine L;
                L.address = addr;
                L.rawLabel = "";
                L.instr = "noop";
                L.f0.clear(); L.f1.clear(); L.f2.clear();
                ir.push_back(L);
                addr++;
                continue;
            }
        }

        // ---------- บรรทัดที่มี tokens ----------
        IRLine L;
        L.address = addr;
        L.f0.clear(); L.f1.clear(); L.f2.clear();

        string first = toks[0];
        bool firstIsMnemonic = (MNEMONICS.find(first) != MNEMONICS.end());

        if (!firstIsMnemonic) {
            // ----- มี label นำหน้า -----
            if (!validLabelName(first)) {
                throw runtime_error("invalid label name '" + first + "' at source line " + to_string(lineno+1));
            }
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

            // ถ้ามีแค่ label ไม่มีคำสั่ง → ข้ามบรรทัดนี้ (ไม่กิน address)
            if (L.instr.empty()) {
                continue;
            }
        } else {
            // ----- ไม่มี label (ขึ้นต้นด้วย mnemonic) -----
            L.rawLabel = "";
            L.instr = first;
            if (toks.size() >= 2) L.f0 = toks[1];
            if (toks.size() >= 3) L.f1 = toks[2];
            if (toks.size() >= 4) L.f2 = toks[3];
        }

        // ป้องกันคำเกิน (เช่นคอมเมนต์ที่ไม่ถูกตัดออก) — ถ้ายังส่ง commentChars เข้ามาถูกต้องจะไม่เกิด
        if (toks.size() > 5 && L.instr != ".fill") {
            // เกินจาก 3 ฟิลด์ของคำสั่ง → ถือว่าเป็นคอมเมนต์/คำเกินที่ยังไม่ถูกตัด
            // ไม่ throw เพื่อความยืดหยุ่น แต่ถ้าต้องการเข้มงวดให้ throw ได้
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
        
        if (m.empty()) {
            // should not happen: in pass1 we either skipped blank lines or converted to noop
            throw runtime_error("missing instruction at address " + to_string(L.address));
        }
        if (MNEMONICS.find(m) == MNEMONICS.end()) {
            throw runtime_error("invalid opcode '" + m + "' at address " + to_string(L.address));
        }

        // .fill
        if (m == ".fill") {
            L.isFill = true;
            if (L.f0.empty()) throw runtime_error(".fill without operand at address " + to_string(L.address));
            if (isNumber(L.f0)) {
                long long v = stoll(L.f0);
                L.fillValue = static_cast<int>(v);
            } else {
                if (labelToAddr.find(L.f0) == labelToAddr.end()) throw runtime_error("undefined label '" + L.f0 + "' used in .fill at address " + to_string(L.address));
                L.fillValue = labelToAddr[L.f0];
            }
            continue;
        }

        // R-type: add, nand
        if (m == "add" || m == "nand") {
            if (L.f0.empty() || L.f1.empty() || L.f2.empty()) throw runtime_error("R-type instruction missing field at address " + to_string(L.address));
            if (!isNumber(L.f0) || !isNumber(L.f1) || !isNumber(L.f2)) throw runtime_error("R-type registers must be numeric at address " + to_string(L.address));
            L.regA = stoi(L.f0);
            L.regB = stoi(L.f1);
            L.dest = stoi(L.f2);
            if (L.regA < 0 || L.regA > 7 || L.regB < 0 || L.regB > 7 || L.dest < 0 || L.dest > 7)
                throw runtime_error("register out of range (0..7) at address " + to_string(L.address));
            continue;
        }

        if (m == "lw" || m == "sw") {
            if (L.f0.empty() || L.f1.empty() || L.f2.empty()) throw runtime_error("lw/sw missing field at address " + to_string(L.address));
            if (!isNumber(L.f0) || !isNumber(L.f1)) throw runtime_error("lw/sw regA/regB must be numeric at address " + to_string(L.address));
            L.regA = stoi(L.f0);
            L.regB = stoi(L.f1);
            if (L.regA < 0 || L.regA > 7 || L.regB < 0 || L.regB > 7) throw runtime_error("register out of range (0..7) at address " + to_string(L.address));
            if (isNumber(L.f2)) {
                long long v = stoll(L.f2);
                if (v < -32768 || v > 32767) throw runtime_error("offset out of 16-bit range for lw/sw at address " + to_string(L.address));
                L.offset16 = static_cast<int>(v);
            } else {
                if (labelToAddr.find(L.f2) == labelToAddr.end()) throw runtime_error("undefined label '" + L.f2 + "' used in lw/sw at address " + to_string(L.address));
                int addrLabel = labelToAddr[L.f2];
                if (addrLabel < -32768 || addrLabel > 32767) throw runtime_error("label address out of 16-bit range for lw/sw at address " + to_string(L.address));
                L.offset16 = addrLabel;
            }
            continue;
        }

        if (m == "beq") {
            if (L.f0.empty() || L.f1.empty() || L.f2.empty()) throw runtime_error("beq missing field at address " + to_string(L.address));
            if (!isNumber(L.f0) || !isNumber(L.f1)) throw runtime_error("beq regA/regB must be numeric at address " + to_string(L.address));
            L.regA = stoi(L.f0);
            L.regB = stoi(L.f1);
            if (L.regA < 0 || L.regA > 7 || L.regB < 0 || L.regB > 7) throw runtime_error("register out of range (0..7) at address " + to_string(L.address));
            if (isNumber(L.f2)) {
                long long v = stoll(L.f2);
                if (v < -32768 || v > 32767) throw runtime_error("beq numeric offset out of 16-bit range at address " + to_string(L.address));
                L.offset16 = static_cast<int>(v);
            } else {
                if (labelToAddr.find(L.f2) == labelToAddr.end()) throw runtime_error("undefined label '" + L.f2 + "' used in beq at address " + to_string(L.address));
                int addrLabel = labelToAddr[L.f2];
                long long offset = static_cast<long long>(addrLabel) - (static_cast<long long>(L.address) + 1LL);
                if (offset < -32768 || offset > 32767) throw runtime_error("beq offset out of range for label '" + L.f2 + "' at address " + to_string(L.address));
                L.offset16 = static_cast<int>(offset);
            }
            continue;
        }

        if (m == "jalr") {
            if (L.f0.empty() || L.f1.empty()) throw runtime_error("jalr missing field at address " + to_string(L.address));
            if (!isNumber(L.f0) || !isNumber(L.f1)) throw runtime_error("jalr registers must be numeric at address " + to_string(L.address));
            L.regA = stoi(L.f0);
            L.regB = stoi(L.f1);
            if (L.regA < 0 || L.regA > 7 || L.regB < 0 || L.regB > 7) throw runtime_error("register out of range (0..7) at address " + to_string(L.address));
            continue;
        }

        // halt, noop ไม่มี field
        if (m == "halt" || m == "noop") {
            // no fields required
            continue;
        }

        // unreachable
        throw runtime_error("unhandled instruction '" + m + "' at address " + to_string(L.address));
    }
}

void Parser::parseFile(const string &filename, bool countBlankLines, const string &commentChars) {
    readAllLines(filename, commentChars);
    pass1_buildSymbolTable(countBlankLines);
    pass2_resolve(countBlankLines);
    // build symbols vector is already done in pass1
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
            << setw(10) << L.f2 // <-- ตรงนี้ 10!
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

/*int main() {
    string inputFile = "test(assembly-language).asm";   // ไฟล์ assembly สำหรับเทส

    Parser parser;                   // สร้างอ็อบเจ็กต์ parser
    parser.parseFile(inputFile);     // เรียกฟังก์ชันหลักเพื่ออ่านและแยกข้อมูล

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
    } */

    //parser.parseFile(inputFile, /*countBlankLines=*/false, /*commentChars=*/";#");
    //parser.writeSymbolsFile("symbolsTable.txt"); // สร้างไฟล์ symbol table

    //cout << "\n--------------------------------------------------\n";
    //cout << "Parse success!.\n";
    //cout << "Output written to : program.ir and symbolsTable.txt" ;

    //return 0;
    int main(int argc, char** argv) {
    // เลือกไฟล์อินพุตจาก argv หรือใช้ดีฟอลต์เป็น factorialmem.asm
    string inputFile = (argc >= 2) ? string(argv[1]) : "factorialmem.asm";

    Parser parser;
    try {
        // อย่านับบรรทัดว่าง และตัดคอมเมนต์ที่ขึ้นต้นด้วย ; หรือ #
        parser.parseFile(inputFile, /*countBlankLines=*/false, /*commentChars=*/";#");

        cout << "\nParsing...\n-------------------------------------\n";

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

        parser.writeIRFile("program.ir");
        parser.writeSymbolsFile("symbolsTable.txt");

        cout << "\n--------------------------------------------------\n";
        cout << "Parse success!\n";
        cout << "Output written to : program.ir and symbolsTable.txt\n";
        return 0;
    } catch (const std::exception& e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}


