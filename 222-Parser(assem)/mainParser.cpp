#include "parser.h"
#include <iostream>
#include <iomanip>

using namespace std;

int main() {
    string inputFile = "test.asm";   // ไฟล์ assembly สำหรับเทส

    Parser parser;                   // สร้างอ็อบเจ็กต์ parser
    parser.parseFile(inputFile);     // เรียกฟังก์ชันหลักเพื่ออ่านและแยกข้อมูล

    cout << "Parsing completed successfully!\n\n";

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
    cout << "\n Parsed Instructions:\n";
    auto insts = parser.getIR();
    for (auto &inst : insts) {
        cout << "Address " << setw(3) << inst.address << " | "
             << setw(6) << left << inst.rawLabel << " | "
             << setw(6) << left << inst.instr << " | "
             << setw(6) << left << inst.f0 << " | "
             << setw(6) << left << inst.f1 << " | "
             << setw(6) << left << inst.f2
             << endl;
    }

    cout << "\n Test finished.\n";

    parser.writeIRFile("program.ir");       // สร้างไฟล์ IR สำหรับ assembler
    parser.writeSymbolsFile("symbols.txt"); // สร้างไฟล์ symbol table

    cout << "\n Created File IR finished.\n";

    return 0;
}
