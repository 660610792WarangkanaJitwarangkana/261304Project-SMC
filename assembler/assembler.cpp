// assembler.cpp
// Compile: g++ -std=c++17 assembler.cpp -o assembler


#include "common.h"   
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <unordered_map>
#include <sstream>
#include <vector>
#include <string>
#include <cctype>
#include <iomanip>

using namespace std;

Assembler::Assembler(const vector<IRLine>& ir, const vector<Label>& symbols) {
    this->ir = ir;
    this->symbols = symbols;
}

int Assembler::getOpcode(const string &mnemonic) const {
    if (mnemonic == "add") return 0;
    if (mnemonic == "nand") return 1;
    if (mnemonic == "lw") return 2;
    if (mnemonic == "sw") return 3;
    if (mnemonic == "beq") return 4;
    if (mnemonic == "jalr") return 5;
    if (mnemonic == "halt") return 6;
    if (mnemonic == "noop") return 7;
    if (mnemonic == ".fill") return -1;
    throw runtime_error("Unknown opcode: " + mnemonic);
}

vector<int> Assembler::assembleAll() const {
    vector<int> codes;
    for (const auto &L : ir) {
        int code = 0;
        if (L.isFill) {
            code = encodeFill(L);
        } else if (L.instr == "add" || L.instr == "nand") {
            code = encodeRType(L);
        } else if (L.instr == "lw" || L.instr == "sw" || L.instr == "beq") {
            code = encodeIType(L);
        } else if (L.instr == "jalr") {
            code = encodeJType(L);
        } else if (L.instr == "halt" || L.instr == "noop") {
            code = encodeOType(L);
        } else {
            cerr << "Error: unknown instruction '" << L.instr
                 << "' at address " << L.address << endl;
            exit(1);
        }
        codes.push_back(code);
    }
    return codes;
}

int Assembler::encodeRType(const IRLine &L) const {
    int opcode = getOpcode(L.instr);
    int regA = L.regA & 0x7;
    int regB = L.regB & 0x7;
    int dest = L.dest & 0x7;
    return (opcode << 22) | (regA << 19) | (regB << 16) | dest;
}

int Assembler::encodeIType(const IRLine &L) const {
    int opcode = getOpcode(L.instr);
    int regA = L.regA & 0x7;
    int regB = L.regB & 0x7;
    int offset = L.offset16 & 0xFFFF; // 16-bit signed
    return (opcode << 22) | (regA << 19) | (regB << 16) | offset;
}

int Assembler::encodeJType(const IRLine &L) const {
    int opcode = getOpcode(L.instr);
    int regA = L.regA & 0x7;
    int regB = L.regB & 0x7;
    return (opcode << 22) | (regA << 19) | (regB << 16);
}

int Assembler::encodeOType(const IRLine &L) const {
    int opcode = getOpcode(L.instr);
    return (opcode << 22);
}

int Assembler::encodeFill(const IRLine &L) const {
    return L.fillValue;
}

void Assembler::writeMachineFile(const string &filename, const vector<int>& codes) const {
    ofstream out(filename);
    if (!out.is_open()) {
        cerr << "Cannot open output file: " << filename << endl;
        exit(1);
    }
    for (int c : codes) out << c << "\n";
    out.close();
}

// ตรวจสอบว่า string เป็นตัวเลขหรือไม่
bool isNumber(const string &s) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '-' || s[0] == '+') i = 1;
    for (; i < s.size(); ++i)
        if (!isdigit(s[i])) return false;
    return true;
}

// หา address ของ label จาก symbol table
int findLabelAddress(const vector<Label> &symbols, const string &label) {
    for (auto &L : symbols)
        if (L.name == label) return L.address;
    cerr << "Undefined label: " << label << endl;
    exit(1);
}

// อ่าน IR file และ resolve label/offset
vector<IRLine> readIRFile(const string &filename, const vector<Label> &symbols) {
    vector<IRLine> ir;
    ifstream in(filename);
    if (!in.is_open()) throw runtime_error("Cannot open IR file: " + filename);

    cout << "\n-------------------------------------\n";
    cout << "\nReading IR from: " << filename << endl;

    string line;
    getline(in, line); // skip header

    int lineCount = 0;
    while (getline(in, line)) {
        if (line.empty()) continue;

        // แยก token ทั้งหมดในบรรทัด
        istringstream iss(line);
        vector<string> tokens;
        string tok;
        while (iss >> tok) tokens.push_back(tok);

        if (tokens.size() < 3) continue; // skip บรรทัดว่าง

        IRLine L{};
        L.address = stoi(tokens[0]);
        int pos = 1;

        // ตรวจว่ามี label หรือไม่ (เช็คว่าถัดจาก address เป็น opcode หรือ label)
        string maybeInstr = tokens[pos];
        vector<string> validInstr = {"add","nand","lw","sw","beq","jalr","halt","noop",".fill"};

        auto isInstr = [&](const string &x) {
            for (auto &i : validInstr) if (x == i) return true;
            return false;
        };

        if (!isInstr(maybeInstr)) { // มี label
            L.rawLabel = maybeInstr;
            pos++;
        }

        L.instr = tokens[pos++];
        L.f0 = (pos < tokens.size()) ? tokens[pos++] : "";
        L.f1 = (pos < tokens.size()) ? tokens[pos++] : "";
        L.f2 = (pos < tokens.size()) ? tokens[pos++] : "";

        // ประมวลผล field ตามประเภทคำสั่ง
        if (L.instr == "add" || L.instr == "nand") {
            L.regA = stoi(L.f0);
            L.regB = stoi(L.f1);
            L.dest = stoi(L.f2);
        } else if (L.instr == "lw" || L.instr == "sw" || L.instr == "beq") {
            L.regA = stoi(L.f0);
            L.regB = stoi(L.f1);
            try {
                L.offset16 = stoi(L.f2);
            } catch (...) {
                // ถ้าเป็น label เช่น "five"
                for (auto &s : symbols) {
                    if (s.name == L.f2) {
                        L.offset16 = s.address;
                        break;
                    }
                }
            }
        } else if (L.instr == ".fill") {
            try {
                L.fillValue = stoi(L.f0);
            } catch (...) {
                for (auto &s : symbols) {
                    if (s.name == L.f0) {
                        L.fillValue = s.address;
                        break;
                    }
                }
            }
            L.isFill = true;
        } else if (L.instr == "jalr") {
            L.regA = stoi(L.f0);
            L.regB = stoi(L.f1);
        }

        ir.push_back(L);
        lineCount++;
    }

    cout << "Loaded " << ir.size() << " IR instructions.\n";
    cout << "\n-------------------------------------\n\n";
    return ir;
}

// อ่าน symbol table
vector<Label> readSymbols(const string &filename) {
    vector<Label> symbols;
    ifstream in(filename);
    if (!in.is_open()) throw runtime_error("Cannot open symbols file: " + filename);
    
    string line;
    getline(in, line); //ข้าม header "LabelName Address"
    
    string name; 
    int addr;
    while (in >> name >> addr)
        symbols.push_back({name, addr});
    in.close();

    cout << "\nLoaded " << symbols.size() << " symbols from: " << filename << " \n " << endl;
    cout << "Symbols : \n";
    for (auto &s : symbols)
        cout << left << s.name << " = " << s.address << endl;

    return symbols;
    
}


int main() {
    try {
        // โหลดข้อมูล symbol และ IR
        vector<Label> symbols = readSymbols("symbolsTable.txt");
        vector<IRLine> ir = readIRFile("program.ir", symbols);

        // สร้าง assembler และแปลงเป็น machine code
        Assembler assem(ir, symbols);
        vector<int> codes = assem.assembleAll();

        cout << "Machine code output:\n";
        for (size_t i = 0; i < codes.size(); ++i)
            cout << setw(3) << i << ": " << setw(12) << codes[i]
                 << "   (0x" << hex << uppercase << codes[i] << dec << ")\n";
        cout << "\n===================================\n";

        // เขียนลงไฟล์
        assem.writeMachineFile("machineCode.mc", codes);
        cout << "Assembling completed successfully!.\n";
        cout << "Output written to: machineCode.mc\n";
        cout << "===================================\n";

    } catch (const exception &e) {
        cerr << "Exception: " << e.what() << endl;
        return 1;
    }
    return 0;
}
