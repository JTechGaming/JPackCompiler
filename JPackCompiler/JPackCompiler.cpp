#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "Codegen.h"
#include "Lexer.h"
#include "Parser.h"

std::string resolveIncludes(const std::string& source, const std::filesystem::path& basePath) {
    std::string result;
    std::istringstream stream(source);
    std::string line;
    while (std::getline(stream, line)) {
        // check for #include directive
        size_t pos = line.find("#include");
        if (pos != std::string::npos) {
            // extract filename between quotes
            size_t start = line.find('"', pos);
            size_t end = line.find('"', start + 1);
            if (start != std::string::npos && end != std::string::npos) {
                std::string filename = line.substr(start + 1, end - start - 1);
                auto includePath = basePath / filename;
                // read and recursively resolve the included file
                std::ifstream includeFile(includePath);
                if (includeFile.is_open()) {
                    std::string includeSource((std::istreambuf_iterator<char>(includeFile)),
                                              std::istreambuf_iterator<char>());
                    result += resolveIncludes(includeSource, includePath.parent_path());
                } else {
                    std::cerr << "Error: could not open include file: " << includePath << "\n";
                }
            }
        } else {
            result += line + "\n";
        }
    }
    return result;
}

int main(int argc, char *argv[]){
    if (argc < 2) {
        std::cerr << "Usage: JPackCompiler <sourcefile>\n";
        return 1;
    }
    std::vector<std::string> inputFiles;
    std::string outputPath = "output";
    bool debugMode = false;
    bool force = false;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-i" || arg == "--input") {
            inputFiles.emplace_back(argv[++i]);
        } else if (arg == "-o" || arg == "--output") {
            outputPath = argv[++i];
        } else if (arg == "-d" || arg == "--debug") {
            debugMode = true;
        } else if (arg == "-f" || arg == "--force") {
            force = true;
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: JPackCompiler -i <file> [-i <file> ...] -o <output> [-d]\n";
            std::cout << "  -i, --input   Input .jpack file (can be specified multiple times)\n";
            std::cout << "  -o, --output  Output directory (default: output)\n";
            std::cout << "  -d, --debug   Enable debug output\n";
            std::cout << "  -f, --force   Force override output\n";
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
        source += resolveIncludes(fileSource, std::filesystem::path(filePath).parent_path());
    }
    
    std::cout << "Tokenizing Code...\n";
    
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    if (debugMode) {
        for (auto token : tokens) {
            std::cout << "token: " << tokenTypeToString(token.type) << " with value: "
                << token.value << " at column: "
                << token.column << " , line: " << token.line << "\n";
        }
    }
    
    std::cout << "Parsing Code...\n";
    
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
    
    std::string namespaceName = std::filesystem::path(inputFiles[0]).stem().string();
    
    if (!force && std::filesystem::exists(outputPath) && !std::filesystem::is_empty(outputPath)) {
        // check if it looks like it belongs to this pack
        bool isSamePack = true;
        for (const auto& entry : std::filesystem::directory_iterator(outputPath)) {
            std::string name = entry.path().filename().string();
            if (name != "pack.mcmeta" && name != "data") {
                isSamePack = false;
                break;
            }
        }
        // check that the data folder only contains this namespace and minecraft
        if (isSamePack) {
            auto dataPath = std::filesystem::path(outputPath) / "data";
            if (std::filesystem::exists(dataPath)) {
                for (const auto& entry : std::filesystem::directory_iterator(dataPath)) {
                    std::string name = entry.path().filename().string();
                    std::string namespaceLower = namespaceName;
                    std::ranges::transform(namespaceLower, namespaceLower.begin(),
                        [](unsigned char c) { return std::tolower(c); });
                    if (name != namespaceLower && name != "minecraft") {
                        isSamePack = false;
                        break;
                    }
                }
            }
        }
        if (!isSamePack) {
            std::cerr << "Error: output directory exists and does not appear to be a JPack output directory. Use -f to force overwrite.\n";
            return 1;
        }
    }
    
    std::cout << "Generating Code...\n";
    
    Codegen codegen(program.get(), namespaceName, debugMode);
    codegen.generate(outputPath);
    
    std::cout << "Successfully compiled JPack program to output directory: " << outputPath << "\n";
}
