#pragma once
#include <memory>
#include <string>
#include <vector>
#include <optional>

#include "Token.h"

struct ASTNode {
    virtual ~ASTNode() = default;
};

struct TypeInfo {
    std::string name;
    bool isReference = false; // for int&
};

struct ParameterNode : ASTNode {
    TypeInfo type;
    std::string name;
};

struct AnnotationNode : ASTNode {
    std::string name;
    std::optional<std::string> argument; // for instance @event("minecraft:player_hurt")
};

struct FunctionNode : ASTNode {
    std::string name;
    TypeInfo returnType;
    std::vector<std::unique_ptr<ParameterNode>> parameters;
    std::vector<std::unique_ptr<ASTNode>> body;
    std::vector<std::unique_ptr<AnnotationNode>> annotations;
};

struct ClassNode : ASTNode {
    std::string name;
    std::vector<std::unique_ptr<ASTNode>> privateMembers;
    std::vector<std::unique_ptr<ASTNode>> publicMembers;
};

struct StructNode : ASTNode {
    std::string name;
    std::vector<std::unique_ptr<ASTNode>> members;
};

struct EnumMember {
    std::string name;
    std::optional<int> value; // for enum E { A = 5 }
};

struct EnumNode : ASTNode {
    std::string name;
    std::vector<EnumMember> members;
};

struct VariableNode : ASTNode {
    TypeInfo type;
    std::string name;
    std::unique_ptr<ASTNode> value;
};

struct IfNode : ASTNode {
    std::unique_ptr<ASTNode> condition;
    std::vector<std::unique_ptr<ASTNode>> body;
    std::vector<std::unique_ptr<ASTNode>> elseBody;
};

struct WhileNode : ASTNode {
    std::unique_ptr<ASTNode> condition;
    std::vector<std::unique_ptr<ASTNode>> body;
};

struct ForNode : ASTNode {
    std::unique_ptr<ASTNode> initializer;
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode> increment;
    std::vector<std::unique_ptr<ASTNode>> body;
};

struct ReturnNode : ASTNode {
    std::unique_ptr<ASTNode> returnVal;
};

struct BinaryExprNode : ASTNode {
    std::unique_ptr<ASTNode> left;
    TokenType op;
    std::unique_ptr<ASTNode> right;
};

struct UnaryExprNode : ASTNode {
    TokenType op;
    std::unique_ptr<ASTNode> operand;
};

struct LiteralNode : ASTNode {
    TokenType type;
    std::string value;
};

struct BreakNode : ASTNode {};
struct ContinueNode : ASTNode {};

struct IdentifierNode : ASTNode {
    std::string name;
};

struct CallNode : ASTNode {
    std::string name;
    std::vector<std::unique_ptr<ASTNode>> arguments;
};

struct MemberAccessNode : ASTNode {
    std::unique_ptr<ASTNode> object;
    std::string memberName;
};

struct ProgramNode : ASTNode {
    std::vector<std::unique_ptr<ASTNode>> declarations;
};