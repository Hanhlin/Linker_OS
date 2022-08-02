#include <fstream>
#include <iostream>
#include <string.h>
#include <cstring>
#include <stdio.h>
#include <errno.h>
#include <math.h> 
#include <map>
#include <vector>
#include <algorithm>

using namespace std;

#pragma region global variables
int line;
int offsets = 0;
char * token;
string lasttok = "";
char * pretok;
int instNum = 0;
int totalInst = 0;
int countModule = 1;
char linebuffer[1048576];
char delims[] = " \t\n";
map<int, int> moduleBase;  //module number; base address
map<string, int> SymRef;  //symbol; ind 
struct Info {
    string sym;
    int address, moduleN, multidefined, instCount, usedmark, firstdefined;
};
vector<Info> SymTable;
vector<string> usedList;
#pragma endregion global variables

void _parseerror(int errcode) {
    static char const* errstr[] = {
        "NUM_EXPECTED", "SYM_EXPECTED", "ADDR_EXPECTED", "SYM_TOO_LONG",
        "TOO_MANY_DEF_IN_MODULE", "TOO_MANY_USE_IN_MODULE", "TOO_MANY_INSTR"};

    printf("Parse Error line %d offset %d: %s\n", line, offsets, errstr[errcode]);
    exit(EXIT_FAILURE);
}

char* getToken(ifstream& inFile) {

    if (token != NULL) {
        lasttok = string(token);
        token = strtok(NULL, delims);
    }
    
    if (token == NULL) {
        if(!inFile.eof()) {

            int continuBlank = -1;
            do {
                inFile.getline(linebuffer, 1048576);
                line++;
                continuBlank++;
            } while (strlen(linebuffer) == 0 && !inFile.eof());

            token = strtok(linebuffer, delims);

            if (inFile.eof()) {
                line--;
                if (continuBlank > 0) {
                    offsets = 1;
                }
                else {
                    offsets += lasttok.length();
                }
            }
        }
    }
    if(!inFile.eof()){
        offsets = token - linebuffer + 1;
    }
    return token;
}

int readInt(ifstream& inFile) {

    token = getToken(inFile);
    if (token) {
        for (size_t i = 0; i < strlen(token); i++) {       
            if (isdigit(token[i]) == 0) {
                _parseerror(0);        //"NUM_EXPECTED"
            }   
        }
        if (atoi(token) >= pow(2, 30)) {
            _parseerror(0);
        }
        return atoi(token);
    }
    else {
        return -1;
    }
}

string readSymbol(ifstream& inFile) {
    token = getToken(inFile);
    if (token == NULL || !isalpha(token[0])) {
        _parseerror(1);           
    }
    for (size_t i = 1; i < strlen(token); i++) {
        if (!isalnum(token[i])) {
            _parseerror(1);       //"SYM_EXPECTED"
        }
    }
    return string(token);
}

char readIAER(ifstream& inFile) {
    token = getToken(inFile);
    if (token == NULL || strlen(token) != 1 
        || (*token != 'I' && *token != 'A' && *token != 'E' && *token != 'R')) {
        _parseerror(2);     //"ADDR_EXPECTED"
    }
    return *token;
}

void createSymbol(string sym, int val) {

    if (SymRef.find(sym) != SymRef.end()) {
        SymTable[SymRef[sym]].multidefined = 1;
        SymTable.push_back({sym, SymTable[SymRef[sym]].address, countModule, 1, 0, 0, 0});
    }
    else {
        SymTable.push_back({sym, val, countModule, 0, 0, 0, 1});
        SymRef[sym] = SymTable.size() - 1;
    }
}

void createModule() {
    if (countModule == 1) {
        moduleBase[countModule] = 0;
    }
    else {
        moduleBase[countModule] = totalInst;
    }
}

void Pass1(ifstream& inFile) {
    while(true) {
        createModule();

        #pragma region defintion list
        int defcount = readInt(inFile);

        if (inFile.eof()) {
            break;
        }
        if (defcount > 16) {
            _parseerror(4);     //TOO_MANY_DEF_IN_MODULE
        }
        for (int i = 0; i < defcount; i++) {
            string sym = readSymbol(inFile);
            int val = readInt(inFile);      
            createSymbol(sym, val);
        }
        #pragma endregion defintion list

        #pragma region use list
        int usecount = readInt(inFile);

        if (usecount > 16) {
            _parseerror(5);     //TOO_MANY_USE_IN_MODULE
        }
        for (int i = 0; i < usecount; i++) {
            string sym = readSymbol(inFile);
        }
        #pragma endregion use list

        #pragma region program list
        int instcount = readInt(inFile);
        totalInst += instcount;
        if (totalInst > 512) {
            _parseerror(6);     //TOO_MANY_INSTR
        }
        for (int i = 0; i < instcount; i++) {
            char addressmode = readIAER(inFile);
            int op = readInt(inFile);
        }
        #pragma endregion program list     

        #pragma region insert instcount into symTable
        for (auto it = SymTable.cbegin(); it != SymTable.cend(); ++it) {

            int ind = it - SymTable.begin();
            if (SymTable[ind].moduleN == countModule) {
                SymTable[ind].instCount = instcount;
            }
        }
        #pragma endregion insert instcount into symTable

        countModule++;
    }
}

void Pass2(ifstream& inFile) {
    countModule = 1;
    while(true) {
        vector<string> uselistVector;

        #pragma region defintion list
        int defcount = readInt(inFile);

        if (inFile.eof()) {
            break;
        }
        for (int i = 0; i < defcount; i++) {
            string sym = readSymbol(inFile);
            int val = readInt(inFile);
        }
        #pragma endregion defintion list

        #pragma region use list
        int usecount = readInt(inFile);
        
        for (int i = 0; i < usecount; i++) {
            string sym = readSymbol(inFile);
            uselistVector.push_back(sym);
        }
        #pragma endregion use list

        #pragma region program list
        int instcount = readInt(inFile);
        vector<int> usedSymL;

        for (int i = 0; i < instcount; i++) {
            char addressmode = readIAER(inFile);
            int op = readInt(inFile);
            int opcode = op / 1000;
            int operand = op % 1000;
            int uselistSize = uselistVector.size();
            
            switch (addressmode) {
            case 'I':
                if (op >= 10000) {
                    //RULE 10(Error)
                    printf("%03d: %04d Error: Illegal immediate value; treated as 9999\n", instNum, 9999);
                }
                else {
                    printf("%03d: %04d\n", instNum, op);
                }
                break;

            case 'A':
                //RULE 11(Error)
                if (opcode >= 10) {
                    printf("%03d: %04d Error: Illegal opcode; treated as 9999\n", instNum, 9999);
                    instNum++;
                    continue;
                }
                if (operand >= 512) {
                    //RULE 8(Error) 
                    printf("%03d: %04d Error: Absolute address exceeds machine size; zero used\n", instNum, opcode * 1000);
                }
                else {
                    printf("%03d: %04d\n", instNum, op);
                }
                break;

            case 'E':
                if (opcode >= 10) {
                    printf("%03d: %04d Error: Illegal opcode; treated as 9999\n", instNum, 9999);
                    instNum++;
                    continue;
                }
                if (operand + 1 > uselistVector.size()) {
                    //RULE 6(Error) 
                    printf("%03d: %04d Error: External address exceeds length of uselist; treated as immediate\n", instNum, opcode * 1000 + operand);
                }
                else {
                    string sym = uselistVector[operand];
                    if (find(usedSymL.begin(), usedSymL.end(), operand) == usedSymL.end()) {
                        usedSymL.push_back(operand);
                    }

                    if (SymRef.find(sym) == SymRef.end()) {
                        printf("%03d: %04d Error: %s is not defined; zero used\n", instNum, opcode * 1000, sym.c_str());
                    }
                    else {
                        //RULE 4
                        SymTable[SymRef[sym]].usedmark = 1;
                        printf("%03d: %04d\n", instNum, opcode * 1000 + SymTable[SymRef[sym]].address + moduleBase[SymTable[SymRef[sym]].moduleN]);
                    }
                }
                break;

            case 'R':
                if (opcode >= 10) {
                    printf("%03d: %04d Error: Illegal opcode; treated as 9999\n", instNum, 9999);
                    instNum++;
                    continue;
                }
                
                int add = operand + moduleBase[countModule];
                if (operand > instcount) {
                    //RULE 9(Error)
                    printf("%03d: %04d Error: Relative address exceeds module size; zero used\n", instNum, opcode * 1000 + moduleBase[countModule]);
                }
                else {
                    printf("%03d: %04d\n", instNum, opcode * 1000 + add);
                }
                break;
            }

            instNum++;
        }
        #pragma endregion program list

        #pragma region check uselist
        //RULE 7(Warning)
        if (uselistVector.size() != 0) {
            for (int i = 0; i < uselistVector.size(); i++) {
                if (find(usedSymL.begin(), usedSymL.end(), i) == usedSymL.end()) {
                    printf("Warning: Module %d: %s appeared in the uselist but was not actually used\n", countModule, uselistVector.at(i).c_str());
                }
            }
        }
        #pragma endregion check uselist

        countModule++;
    }
}

void checkSymbolSize() {
    for (auto it = SymTable.cbegin(); it != SymTable.cend(); ++it) {
        int ind = it - SymTable.begin();
            
        if (SymTable[ind].address > SymTable[ind].instCount - 1) {
            if (SymTable[ind].multidefined == 1 && SymTable[ind].firstdefined == 1) {
                printf("Warning: Module %d: %s too big %d (max=%d) assume zero relative\n"
                        , SymTable[ind].moduleN, SymTable[ind].sym.c_str(), SymTable[ind].address, SymTable[ind].instCount - 1);
                    
                SymTable[ind].address = 0;
            }
            else if (SymTable[ind].multidefined == 1 && SymTable[ind].firstdefined == 0) {
                if (SymTable[ind].instCount <= 0 && SymTable[SymRef[SymTable[ind].sym]].instCount <= 0) {
                    printf("Warning: Module %d: %s too big %d (max=%d) assume zero relative\n"
                            , SymTable[ind].moduleN, SymTable[ind].sym.c_str(), SymTable[SymRef[SymTable[ind].sym]].address, SymTable[ind].instCount - 1);

                    SymTable[ind].address = 0;
                }
            }
            else {
                printf("Warning: Module %d: %s too big %d (max=%d) assume zero relative\n"
                        , SymTable[ind].moduleN, SymTable[ind].sym.c_str(), SymTable[ind].address, SymTable[ind].instCount - 1);
                    
                SymTable[ind].address = 0;
            }
        }
    }
}

void printSymTable() {
    for (auto it = SymTable.cbegin(); it != SymTable.cend(); ++it) {
        int ind = it - SymTable.begin();

        if (SymTable[ind].multidefined == 1 && SymTable[ind].firstdefined == 1) {
            printf("%s=%d Error: This variable is multiple times defined; first value used\n"
                    ,SymTable[ind].sym.c_str(), SymTable[ind].address + moduleBase[SymTable[ind].moduleN]);
        }
        else if (SymTable[ind].multidefined == 0 && SymTable[ind].firstdefined == 1) {
            printf("%s=%d\n",SymTable[ind].sym.c_str(), SymTable[ind].address + moduleBase[SymTable[ind].moduleN]);
        }
    }
}

void checkUsedSym() {
    printf("\n");
        for(auto it = SymTable.begin(); it != SymTable.end(); it++) {
            
            int ind = it - SymTable.begin();
            if (SymRef[SymTable[ind].sym] == ind && SymTable[ind].usedmark == 0) {
                    printf("Warning: Module %d: %s was defined but never used\n", SymTable[ind].moduleN, SymTable[ind].sym.c_str());
            }
        }
    printf("\n");
}

int main(int argc, char* argv[]) {

    ifstream inFile(argv[1]);
    ifstream inFile2(argv[1]);
    if(!inFile.is_open()) {
        printf("%s: %s\n", strerror(errno), argv[1]);
        exit(EXIT_FAILURE);
    }

    setbuf(stdout, NULL);

    Pass1(inFile);
    checkSymbolSize();
    printf("Symbol Table\n");
    printSymTable();
    printf("\nMemory Map\n");
    Pass2(inFile2);
    checkUsedSym();
    
    return 0;
}

