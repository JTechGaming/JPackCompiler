#include <iostream>
#include <string>

#include "Lexer.h"

int main() {
    Lexer lexer("// test\nvoid hello(string msg) { }");
    auto tokens = lexer.tokenize();
    for (auto token : tokens) {
        std::cout << "token: " << tokenTypeToString(token.type) << " with value: "
            << token.value << " at column: "
            << token.column << " , line: " << token.line << "\n";
    }
}
