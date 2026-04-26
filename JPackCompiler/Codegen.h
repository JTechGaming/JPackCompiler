#pragma once
#include "AST.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>

struct ExprResult {
    std::string commands;
    std::string resultEntry;
    bool isStorage = false; // true = NBT storage path, false = scoreboard entry
};

struct ConditionResult {
    std::string commands;   // setup commands
    std::string condition;  // "if score test left = test right"
    std::string elseCondition;
};

class Codegen {
public:
    Codegen(ProgramNode* programNode, std::string name) : m_programNode(programNode), m_name(std::move(name)) {}

    void generate(std::string outputPath);

private:
    ProgramNode* m_programNode;
    std::string m_name;
    std::string m_prefix;
    std::string m_outputPath;
    std::vector<FunctionNode*> m_loadFunctions;
    std::vector<FunctionNode*> m_tickFunctions;
    std::unordered_map<std::string, std::string> m_scoreboardNames;
    std::unordered_map<std::string, std::string> m_storageNames;
    std::unordered_map<std::string, std::vector<std::string>> m_functionParams;
    std::unordered_map<std::string, std::vector<std::string>> m_functionParamNames;
    std::unordered_map<std::string, std::vector<std::string>> m_functionParamTypes;
    std::unordered_set<std::string> m_intrinsicFunctions;
    int m_counter = 1;
    
    void generateFunction(FunctionNode* function);
    std::string generateStatement(ASTNode* node);
    ExprResult generateExpression(ASTNode* node);
    std::string generateSubFunction(const std::string& name, const std::vector<std::unique_ptr<ASTNode>>& body);
    ConditionResult generateCondition(ASTNode* node);
    void registerFunctions();
    void generatePackMeta() const;
    void generateTickJson() const;
    void generateLoadJson() const;
    std::string makeScoreboardName(const std::string& functionName, const std::string& varName);
    std::string makeUUID();
    void formatPrefix();
};
