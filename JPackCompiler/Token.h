#pragma once
#include <string>

enum class TokenType {
    // Literals
    INT_LITERAL, FLOAT_LITERAL, DOUBLE_LITERAL, STRING_LITERAL, BOOL_LITERAL,

    // Types
    INT, FLOAT, DOUBLE, BOOL, STRING, VOID,

    // Keywords
    CLASS, STRUCT, ENUM,
    IF, ELSE, WHILE, FOR, RETURN, BREAK, CONTINUE,
    PUBLIC, PRIVATE,

    // Symbols
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACK, RBRACK, SEMICOLON, COLON, DOUBLE_COLON, COMMA, DOT,

    // Operators
    PLUS, MINUS, TIMES, DIVIDE, MOD,
    INCREMENT, DECREMENT,
    AMPERSAND,

    // Comparison
    EQUAL, EQUAL_EQUAL, NOT_EQUAL, LESSER, GREATER, LESSER_EQUAL, GREATER_EQUAL,

    // Logical
    AND, OR, NOT,

    // Special
    AT, IDENTIFIER,

    END_OF_FILE
};

inline std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::INT_LITERAL:   return "INT_LITERAL";
        case TokenType::FLOAT_LITERAL: return "FLOAT_LITERAL";
        case TokenType::DOUBLE_LITERAL:  return "DOUBLE_LITERAL";
        case TokenType::STRING_LITERAL:  return "STRING_LITERAL";
        case TokenType::BOOL_LITERAL:  return "BOOL_LITERAL";
        case TokenType::INT:  return "INT";
        case TokenType::FLOAT:  return "FLOAT";
        case TokenType::DOUBLE:  return "DOUBLE";
        case TokenType::BOOL:  return "BOOL";
        case TokenType::STRING:  return "STRING";
        case TokenType::VOID:  return "VOID";
        case TokenType::CLASS:  return "CLASS";
        case TokenType::STRUCT:  return "STRUCT";
        case TokenType::ENUM:  return "ENUM";
        case TokenType::IF:  return "IF";
        case TokenType::ELSE:  return "ELSE";
        case TokenType::WHILE:  return "WHILE";
        case TokenType::FOR:  return "FOR";
        case TokenType::RETURN:  return "RETURN";
        case TokenType::BREAK:  return "BREAK";
        case TokenType::CONTINUE:  return "CONTINUE";
        case TokenType::PUBLIC:  return "PUBLIC";
        case TokenType::PRIVATE:  return "PRIVATE";
        case TokenType::LPAREN:  return "LPAREN";
        case TokenType::RPAREN:  return "RPAREN";
        case TokenType::LBRACE:  return "LBRACE";
        case TokenType::RBRACE:  return "RBRACE";
        case TokenType::LBRACK:  return "LBRACK";
        case TokenType::RBRACK:  return "RBRACK";
        case TokenType::SEMICOLON:  return "SEMICOLON";
        case TokenType::COLON:  return "COLON";
        case TokenType::DOUBLE_COLON:  return "DOUBLE_COLON";
        case TokenType::COMMA:  return "COMMA";
        case TokenType::DOT:  return "DOT";
        case TokenType::PLUS:  return "PLUS";
        case TokenType::MINUS:  return "MINUS";
        case TokenType::TIMES:  return "TIMES";
        case TokenType::DIVIDE:  return "DIVIDE";
        case TokenType::MOD:  return "MOD";
        case TokenType::INCREMENT:  return "INCREMENT";
        case TokenType::DECREMENT:  return "DECREMENT";
        case TokenType::AMPERSAND:  return "AMPERSAND";
        case TokenType::EQUAL:  return "EQUAL";
        case TokenType::EQUAL_EQUAL:  return "EQUAL_EQUAL";
        case TokenType::NOT_EQUAL:  return "NOT_EQUAL";
        case TokenType::LESSER:  return "LESSER";
        case TokenType::GREATER:  return "GREATER";
        case TokenType::LESSER_EQUAL:  return "LESSER_EQUAL";
        case TokenType::GREATER_EQUAL:  return "GREATER_EQUAL";
        case TokenType::AND:  return "AND";
        case TokenType::OR:  return "OR";
        case TokenType::NOT:  return "NOT";
        case TokenType::AT:  return "AT";
        case TokenType::IDENTIFIER:  return "IDENTIFIER";
        case TokenType::END_OF_FILE:  return "END_OF_FILE";
    }
    return "";
}

struct Token {
    TokenType type;
    std::string value;
    int line, column;
};
