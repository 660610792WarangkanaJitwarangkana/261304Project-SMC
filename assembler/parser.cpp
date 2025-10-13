// parser.cpp
// Compile: g++ -std=c++17 parser.cpp -o parser

#include "common.h"
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

static const unordered_set<string> MNEMONICS = {
    "add","nand","lw","sw","beq","jalr","halt","noop", ".fill"
};

bool isNumber(const string &s) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '+' || s[0] == '-') i = 1;
    if (i == s.size()) return false;
    for (; i < s.size(); ++i) if (!isdigit((unsigned char)s[i])) return false;
    return true;
}

static bool validLabelName(const string &s) {
    if (s.empty()) return false;
    if (!isalpha((unsigned char)s[0])) return false;
    if (s.size() > 6) return false;
    for (char c: s) if (!isalnum((unsigned char)c)) return false;
    return true;
}

void dieError(const string &msg) {
    cerr << "Error: " << msg << "\n";
    exit(1);
}

static vector<string> tokenize_ws(const string &s) {
    vector<string> toks;
    istringstream iss(s);
    string t;
    while (iss >> t) toks.push_back(t);
    return toks;
}


Parser::Parser() {}

void Parser::readAllLines(const string &filename, const string &commentChars) {
    rawLines.clear();
    ifstream ifs(filename);
    if (!ifs.is_open()) throw runtime_error("cannot open input file: " + filename);
    string line;
    while (getline(ifs, line)) {
        // strip newline already done; remove comment starting at any of commentChars
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
        // keep raw line (may be blank or whitespace-only)
        rawLines.push_back(line);
    }
    ifs.close();
}


void Parser::pass1_buildSymbolTable(bool countBlankLines) {
    ir.clear();
    symbols.clear();
    unordered_map<string,int> labelToAddr;
    int addr = 0;
    for (size_t lineno = 0; lineno < rawLines.size(); ++lineno) {
        string line = rawLines[lineno];
        // Tokenize
        vector<string> toks = tokenize_ws(line);
        bool isBlank = toks.empty();
        IRLine L;
        L.address = addr;

        if (isBlank) {
            if (countBlankLines) {
                // treat blank line as noop (instr="noop"), no label
                L.instr = "noop";
                L.rawLabel = "";
            } else {
                // skip blanks entirely (do not consume address)
                continue;
            }
        } else {
            // non-blank tokens exist. Determine if first token is label or mnemonic
            string first = toks[0];
            bool firstIsMnemonic = (MNEMONICS.find(first) != MNEMONICS.end());
            if (!firstIsMnemonic) {
                // label present
                if (!validLabelName(first)) {
                    throw runtime_error("invalid label name '" + first + "' at source line " + to_string(lineno+1));
                }
                if (labelToAddr.find(first) != labelToAddr.end()) {
                    throw runtime_error("duplicate label '" + first + "' at source line " + to_string(lineno+1));
                }
                labelToAddr[first] = addr;
                symbols.push_back({first, addr});
                L.rawLabel = first;
                // instruction and fields follow if present
                if (toks.size() >= 2) L.instr = toks[1];
                if (toks.size() >= 3) L.f0 = toks[2];
                if (toks.size() >= 4) L.f1 = toks[3];
                if (toks.size() >= 5) L.f2 = toks[4];
            } else {
                // no label
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

void Parser::pass2_resolve(bool countBlankLines) {
    // build label map for fast lookup
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
    string inputFile = "test(assembly-language).asm";   // ไฟล์ assembly สำหรับเทส

    Parser parser;                   // สร้างอ็อบเจ็กต์ parser
    parser.parseFile(inputFile);     // เรียกฟังก์ชันหลักเพื่ออ่านและแยกข้อมูล

    cout << "\n\nParsing...";
    cout << "\n-------------------------------------\n";

    // -------------------------------------------------------------
    // แสดง Symbol Table
    // -------------------------------------------------------------
    cout << "Symbol Table:\n";
    for (auto &sym : parser.getSymbols()) {
        cout << "  " << setw(10) << left << sym.name
             << "-> Address: " << sym.address << endl;
    }

    // -------------------------------------------------------------
    // แสดงผล Instruction ทั้งหมดที่ parser แยกได้
    // -------------------------------------------------------------
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

    cout << "\nTest finished.\n";

    parser.writeIRFile("program.ir");       // สร้างไฟล์ IR สำหรับ assembler
    parser.writeSymbolsFile("symbolsTable.txt"); // สร้างไฟล์ symbol table

    cout << "\n===============================\n";
    cout << "Parsing completed successfully!.\n";
    cout << "Output written to: program.ir";
    cout << "\n===============================\n";

    return 0;
}
