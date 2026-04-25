#pragma once
#include <memory>
#include <vector>
#include <string>

#include "AST.h"
#include "Token.h"

class Parser {
public:
    explicit Parser(std::vector<Token> m_tokens) : m_tokens(std::move(m_tokens)), m_pos(0) {
    }

    std::unique_ptr<ProgramNode> parse();
    
private:
    std::vector<Token> m_tokens;
    size_t m_pos;
    
    Token current() const;
    Token peek(uint8_t offset = 1) const;
    Token advance(uint8_t offset = 1);
    bool  isAtEnd() const;
    
    std::unique_ptr<ASTNode>        parseDeclaration();
    std::unique_ptr<FunctionNode>   parseFunctionDecl(std::vector<std::unique_ptr<AnnotationNode>> annotations);
    std::unique_ptr<ClassNode>      parseClassDecl();
    std::unique_ptr<ASTNode>        parseStatement();
    std::unique_ptr<ASTNode>        parseIfStatement();
    std::unique_ptr<ASTNode>        parseWhileStatement();
    std::unique_ptr<ASTNode>        parseForStatement();
    std::unique_ptr<ASTNode>        parseExpression(int minBP = 0);
    std::unique_ptr<StructNode>     parseStructDecl();
    std::unique_ptr<EnumNode>       parseEnumDecl();
    std::unique_ptr<VariableNode>   parseVariableDecl();
    std::unique_ptr<AnnotationNode> parseAnnotation();
    
    std::vector<std::unique_ptr<ASTNode>> parseBlock();
    
    std::unique_ptr<ASTNode> parsePrimary();
    int getBindingPower(TokenType type);
    
    Token expect(TokenType type);
    bool  match (TokenType type);
};
