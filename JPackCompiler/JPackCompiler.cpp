#include <fstream>
#include <iostream>
#include <string>

#include "Lexer.h"
#include "Parser.h"

int main(int argc, char *argv[]){
    if (argc < 2) {
        std::cerr << "Usage: JPackCompiler <sourcefile>\n";
        return 1;
    }
    std::ifstream file;
    file.open(argv[1]);
    if (!file.is_open()) {
        std::cerr << "Error: could not open file: " << argv[1] << "\n";
        return 1;
    }
    std::string source((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    if (source.size() >= 3 &&
        static_cast<unsigned char>(source[0]) == 0xEF &&
        static_cast<unsigned char>(source[1]) == 0xBB &&
        static_cast<unsigned char>(source[2]) == 0xBF) {
        source = source.substr(3);
    }
    file.close();
    
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    for (auto token : tokens) {
        std::cout << "token: " << tokenTypeToString(token.type) << " with value: "
            << token.value << " at column: "
            << token.column << " , line: " << token.line << "\n";
    }
    
    Parser parser(tokens);
    auto program = parser.parse();
    if (program == nullptr) {
        std::cout << "Parser returned null!\n";
        return 1;
    }
    std::cout << "Declarations: " << program->declarations.size() << "\n";
    parser.printNode(program.get());
}
