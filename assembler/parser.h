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
    string rawLabel;    
    string instr;       
    string f0, f1, f2;  

    bool isFill = false;
    bool hasError = false;
    string errorMsg;

    int regA = 0;
    int regB = 0;
    int dest = 0;
    int offset16 = 0;      
    int fillValue = 0;     
};

class Parser {
public:
    Parser();
    
    void parseFile(const string &filename, bool countBlankLines = false, const string &commentChars = "#;");

    const vector<IRLine>& getIR() const;
    const vector<Label>& getSymbols() const;

    
    void writeIRFile(const string &outname = "program.ir") const;
    void writeSymbolsFile(const string &outname = "program_symbols.txt") const;

private:
    vector<string> rawLines;
    vector<IRLine> ir;
    vector<Label> symbols;

    void readAllLines(const std::string &filename, const std::string &commentChars);
    void pass1_buildSymbolTable(bool countBlankLines);
    void pass2_resolve(bool countBlankLines);
};

#endif 