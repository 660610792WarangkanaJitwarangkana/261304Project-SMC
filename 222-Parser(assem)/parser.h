// parser.h
// Header for assembler front-end (Parser & Symbol Table Manager)
#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <vector>

using namespace std;

struct Label {
    string name;
    int address;
};

struct IRLine {
    int address;
    string rawLabel;    // label text if present ("" otherwise)
    string instr;       // mnemonic (e.g., "add", ".fill") or "" for blank-line-as-noop
    string f0, f1, f2;  // raw field strings (may be empty)

    // resolved (set in pass2)
    bool isFill = false;
    bool hasError = false;
    string errorMsg;

    int regA = 0;
    int regB = 0;
    int dest = 0;
    int offset16 = 0;      // for beq/lw/sw (16-bit signed interpreted as int)
    int fillValue = 0;     // for .fill (32-bit int)
};

// Parser class: two-pass front-end
// Usage:
//   Parser p;
//   p.parseFile("input.asm", /*countBlankLines=*/true, /*commentChars=*/"#;");
//   auto ir = p.getIR(); auto symbols = p.getSymbols();
class Parser {
public:
    Parser();
    // Parse file. Options:
    //  - countBlankLines: when true, treat every source file line as an address (blank-lines become noop entries)
    //  - commentChars: string of characters treated as comment start (each char begins a comment)
    // On error the parser will throw std::runtime_error (with message).
    void parseFile(const string &filename, bool countBlankLines = false, const string &commentChars = "#;");

    const vector<IRLine>& getIR() const;
    const vector<Label>& getSymbols() const;

    // convenience: write IR CSV and symbols text files
    void writeIRFile(const string &outname = "program.ir") const;
    void writeSymbolsFile(const string &outname = "symbols.txt") const;

private:
    vector<string> rawLines;
    vector<IRLine> ir;
    vector<Label> symbols;

    // internal helpers (private)
    void readAllLines(const std::string &filename, const std::string &commentChars);
    void pass1_buildSymbolTable(bool countBlankLines);
    void pass2_resolve(bool countBlankLines);
};

#endif // PARSER_H
