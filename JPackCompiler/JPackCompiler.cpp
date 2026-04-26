#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "Codegen.h"
#include "Lexer.h"
#include "Parser.h"

int main(int argc, char *argv[]){
    if (argc < 2) {
        std::cerr << "Usage: JPackCompiler <sourcefile>\n";
        return 1;
    }
    std::string source;
    for (int i = 1; i < argc - 1; i++) {
        std::ifstream file;
        file.open(argv[i]);
        if (!file.is_open()) {
            std::cerr << "Error: could not open file: " << argv[1] << "\n";
            return 1;
        }
        std::string fileSource((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        if (fileSource.size() >= 3 &&
            static_cast<unsigned char>(fileSource[0]) == 0xEF &&
            static_cast<unsigned char>(fileSource[1]) == 0xBB &&
            static_cast<unsigned char>(fileSource[2]) == 0xBF) {
            fileSource = fileSource.substr(3);
        }
        file.close();
        source += fileSource;
    }
    
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    // for (auto token : tokens) {
    //     std::cout << "token: " << tokenTypeToString(token.type) << " with value: "
    //         << token.value << " at column: "
    //         << token.column << " , line: " << token.line << "\n";
    // }
    
    Parser parser(tokens);
    auto program = parser.parse();
    if (program == nullptr) {
        std::cout << "Parser returned null!\n";
        return 1;
    }
    std::cout << "Declarations: " << program->declarations.size() << "\n";
    //parser.printNode(program.get());
    
    std::string outputPath = argc >= 3 ? argv[argc - 1] : "output";
    
    Codegen codegen(program.get(), std::filesystem::path(argv[1]).stem().string());
    codegen.generate(outputPath);
}
