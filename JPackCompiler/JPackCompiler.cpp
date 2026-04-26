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
    std::vector<std::string> inputFiles;
    std::string outputPath = "output";
    bool debugMode = false;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-i" || arg == "--input") {
            inputFiles.emplace_back(argv[++i]);
        } else if (arg == "-o" || arg == "--output") {
            outputPath = argv[++i];
        } else if (arg == "-d" || arg == "--debug") {
            debugMode = true;
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: JPackCompiler -i <file> [-i <file> ...] -o <output> [-d]\n";
            std::cout << "  -i, --input   Input .jpack file (can be specified multiple times)\n";
            std::cout << "  -o, --output  Output directory (default: output)\n";
            std::cout << "  -d, --debug   Enable debug output\n";
            std::cout << "  -v, --version Print version\n";
            std::cout << "  -h, --help    Print this help\n";
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "JPackCompiler v0.1.0\n";
            return 0;
        }
    }
    
    if (inputFiles.empty()) {
        std::cerr << "Error: no input files specified. Use -i <file>\n";
        return 1;
    }
    
    std::string source;
    for (const auto& filePath : inputFiles) {
        std::ifstream file;
        file.open(filePath);
        if (!file.is_open()) {
            std::cerr << "Error: could not open file: " << filePath << "\n";
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
    if (debugMode) {
        for (auto token : tokens) {
            std::cout << "token: " << tokenTypeToString(token.type) << " with value: "
                << token.value << " at column: "
                << token.column << " , line: " << token.line << "\n";
        }
    }
    
    Parser parser(tokens);
    auto program = parser.parse();
    if (program == nullptr) {
        std::cout << "Parser returned null!\n";
        return 1;
    }
    if (debugMode) {
        std::cout << "Declarations: " << program->declarations.size() << "\n";
        parser.printNode(program.get());
    }
    
    Codegen codegen(program.get(), std::filesystem::path(inputFiles[0]).stem().string(), debugMode);
    codegen.generate(outputPath);
}
