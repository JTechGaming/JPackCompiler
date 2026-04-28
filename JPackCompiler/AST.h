#pragma once
#include <memory>
#include <string>
#include <vector>
#include <optional>

#include "Token.h"

struct ASTNode {
    virtual ~ASTNode() = default;
    int sourceLine = 0;
    int sourceColumn = 0;
};

struct TypeInfo {
    std::string name;
    bool isReference = false; // for int&
    bool isArray = false;
};

struct ParameterNode : ASTNode {
    TypeInfo type;
    std::string name;
};

struct TemplatePoolEntry {
    std::string location;
    int weight;
};

struct AnnotationNode : ASTNode {
    std::string name;
    std::vector<std::string> arguments; // for instance @event("minecraft:player_hurt")
    std::vector<TemplatePoolEntry> poolEntries;
};

struct FunctionNode : ASTNode {
    std::string name;
    TypeInfo returnType;
    std::vector<std::unique_ptr<ParameterNode>> parameters;
    std::vector<std::unique_ptr<ASTNode>> body;
    std::vector<std::unique_ptr<AnnotationNode>> annotations;
    bool isIntrinsic = false;
    bool isReturnsCommand = false;
    bool isStoreResultIntrinsic = false;
    bool isRefIntrinsic = false;
    bool isRevoke = false;
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
    std::vector<std::unique_ptr<ASTNode>> arrayInitializer;
    int arraySize;
};

struct ArrayDeclNode : ASTNode {
    TypeInfo elementType;
    std::string name;
    std::vector<std::unique_ptr<ASTNode>> initializer;
};

struct ArrayAccessNode : ASTNode {
    std::string name;
    std::unique_ptr<ASTNode> index;
};

struct ArrayAssignNode : ASTNode {
    std::string name;
    std::unique_ptr<ASTNode> index;
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
    std::string namespaceName;
    std::string name;
    std::vector<std::unique_ptr<ASTNode>> arguments;
};

struct MemberAccessNode : ASTNode {
    std::unique_ptr<ASTNode> object;
    std::string memberName;
};

struct NamespaceNode : ASTNode {
    std::string name;
    std::vector<std::unique_ptr<ASTNode>> declarations;
};

struct ProgramNode : ASTNode {
    std::vector<std::unique_ptr<ASTNode>> declarations;
};

struct RawCommandNode : ASTNode {
    std::string command; // the raw command string with {param} placeholders
};