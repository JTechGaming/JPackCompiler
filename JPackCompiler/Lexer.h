#pragma once
#include <string>
#include <vector>
#include "Token.h"

class Lexer {
public:
    explicit Lexer(std::string source)
        : m_source(std::move(source)), m_pos(0), m_line(1), m_column(1) {
    }
    
    std::vector<Token> tokenize();

private:
    std::string m_source;
    size_t m_pos;
    int m_line, m_column;
    int m_tokenLine, m_tokenColumn;
    
    std::vector<Token> m_tokens;
    
    char current() const;
    char peek() const;
    char advance();
    
    void addToken(TokenType type, std::string value = "");
    
    void nextToken();
    void skipWhitespace();
    void lexString();
    void lexNumber(char c);
    void lexIdentifier(char c);
    bool isAtEnd() const;
};
