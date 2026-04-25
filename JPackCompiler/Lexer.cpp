#include "Lexer.h"

#include <iostream>
#include <unordered_map>
#include <utility>

static const std::unordered_map<std::string, TokenType> keywords = {
    {"int", TokenType::INT},
    {"float", TokenType::FLOAT},
    {"double", TokenType::DOUBLE},
    {"bool", TokenType::BOOL},
    {"true",  TokenType::BOOL_LITERAL},
    {"false", TokenType::BOOL_LITERAL},
    {"string", TokenType::STRING},
    {"void", TokenType::VOID},
    {"class",  TokenType::CLASS},
    {"struct",  TokenType::STRUCT},
    {"enum",  TokenType::ENUM},
    {"if",  TokenType::IF},
    {"else",  TokenType::ELSE},
    {"while",  TokenType::WHILE},
    {"for",  TokenType::FOR},
    {"return",  TokenType::RETURN},
    {"break",  TokenType::BREAK},
    {"continue",  TokenType::CONTINUE},
    {"public",  TokenType::PUBLIC},
    {"private",  TokenType::PRIVATE},
};

std::vector<Token> Lexer::tokenize() {
    while (!isAtEnd()) {
        skipWhitespace();
        if (!isAtEnd()) nextToken();
    }
    m_tokens.emplace_back(Token{TokenType::END_OF_FILE});
    
    return m_tokens;
}

char Lexer::current() const {
    return isAtEnd() ? '\0' : m_source.at(m_pos);
}

char Lexer::peek() const {
    return m_pos + 1 >= m_source.size() ? '\0' : m_source.at(m_pos + 1);
}

char Lexer::advance() {
    char currentChar = current();
    m_pos++;
    if (currentChar == '\n') {
        m_line++;
        m_column = 1;
    } else {
        m_column++;
    }
    
    return currentChar;
}

void Lexer::addToken(TokenType type, std::string value) {
    m_tokens.emplace_back(Token{type, std::move(value), m_tokenLine, m_tokenColumn});
}

void Lexer::nextToken() {
    m_tokenLine = m_line;
    m_tokenColumn = m_column;
    char c = advance();
    char nc = current();

    if (c == ':' && nc == ':') { addToken(TokenType::DOUBLE_COLON);  advance(); return; }
    if (c == '+' && nc == '+') { addToken(TokenType::INCREMENT);     advance(); return; }
    if (c == '-' && nc == '-') { addToken(TokenType::DECREMENT);     advance(); return; }
    if (c == '=' && nc == '=') { addToken(TokenType::EQUAL_EQUAL);   advance(); return; }
    if (c == '!' && nc == '=') { addToken(TokenType::NOT_EQUAL);     advance(); return; }
    if (c == '<' && nc == '=') { addToken(TokenType::LESSER_EQUAL);  advance(); return; }
    if (c == '>' && nc == '=') { addToken(TokenType::GREATER_EQUAL); advance(); return; }
    if (c == '&' && nc == '&') { addToken(TokenType::AND);           advance(); return; }
    if (c == '|' && nc == '|') { addToken(TokenType::OR);            advance(); return; }
    
    switch (c) {
        case '(': { addToken(TokenType::LPAREN);    break; }
        case ')': { addToken(TokenType::RPAREN);    break; }
        case '{': { addToken(TokenType::LBRACE);    break; }
        case '}': { addToken(TokenType::RBRACE);    break; }
        case '[': { addToken(TokenType::LBRACK);    break; }
        case ']': { addToken(TokenType::RBRACK);    break; }
        case ';': { addToken(TokenType::SEMICOLON); break; }
        case ':': { addToken(TokenType::COLON);     break; }
        case ',': { addToken(TokenType::COMMA);     break; }
        case '.': { addToken(TokenType::DOT);       break; }
        case '+': { addToken(TokenType::PLUS);      break; }
        case '-': { addToken(TokenType::MINUS);     break; }
        case '*': { addToken(TokenType::TIMES);     break; }
        case '/': { addToken(TokenType::DIVIDE);    break; }
        case '%': { addToken(TokenType::MOD);       break; }
        case '&': { addToken(TokenType::AMPERSAND); break; }
        case '=': { addToken(TokenType::EQUAL);     break; }
        case '<': { addToken(TokenType::LESSER);    break; }
        case '>': { addToken(TokenType::GREATER);   break; }
        case '!': { addToken(TokenType::NOT);       break; }
        case '@': { addToken(TokenType::AT);        break; }
        case '"': { lexString(); break; }
        
        default: {
            if (isdigit(c)) {
                lexNumber(c);
            } else if (isalpha(c) || c == '_') {
                lexIdentifier(c);
            } else {
                std::cout << "Lexer ERROR: Unknown character: " << c << "\n";
            }
        }
    }
}

void Lexer::skipWhitespace() {
    while (true) {
        bool found = false;
        while (current() == ' ' || current() == '\t' || current() == '\r' || current() == '\n') {
            advance();
            found = true;
        }
        if (current() == '/' && peek() == '/') {
            while (current() != '\n' && !isAtEnd()) advance();
            found = true;
        } else if (current() == '/' && peek() == '*') {
            advance();
            advance();
            while (!(current() == '*' && peek() == '/') && !isAtEnd()) advance();
            advance();
            advance();
            found = true;
        }
        if (!found) {
            break;
        }
    }
}

void Lexer::lexString() {
    std::string result;
    while (current() != '"' && !isAtEnd()) {
        result += current();
        advance();
    }
    if (isAtEnd()) {
        std::cout << "Lexer ERROR: Unclosed string literal at line " << m_tokenLine << "\n";
    } else {
        advance(); // consume closing "
    }
    
    addToken(TokenType::STRING_LITERAL, result);
}

void Lexer::lexNumber(char c) {
    std::string result(1, c);
    TokenType type = TokenType::INT_LITERAL;
    
    while (isdigit(current())) {
        result += current();
        advance();
    }
    if (current() == '.') {
        type = TokenType::DOUBLE_LITERAL;
        result += '.';
        advance();
        while (isdigit(current())) {
            result += current();
            advance();
        }
    }
    if (current() == 'f') {
        type = TokenType::FLOAT_LITERAL;
        advance();
    }
    
    addToken(type, result);
}

void Lexer::lexIdentifier(char c) {
    std::string result(1, c);
    
    while (isalnum(current()) || current() == '_') {
        result += current();
        advance();
    }
    
    auto it = keywords.find(result);
    if (it != keywords.end()) {
        addToken(it->second);
    } else {
        addToken(TokenType::IDENTIFIER, result);
    }
}

bool Lexer::isAtEnd() const {
    return m_pos >= m_source.size();
}
