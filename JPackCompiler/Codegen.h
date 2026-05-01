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

struct ClassDefinition {
    std::vector<VariableNode*> memberVariables;
    std::vector<FunctionNode*> memberFunctions;
};

struct CompileTimeValue {
    std::string stringValue;
    bool isThis = false;
};

struct CompileTimeInstance {
    std::string className;
    std::unordered_map<std::string, std::string> memberValues;
};

class Codegen {
public:
    Codegen(ProgramNode* programNode, std::string name, bool debugMode) : m_programNode(programNode), m_name(std::move(name)), m_debugMode(debugMode) {}

    void generate(std::string outputPath);

    static std::string serializeJson(const JsonNode* json);

private:
    ProgramNode* m_programNode;
    std::string m_name;
    std::string m_prefix;
    std::string m_outputPath;
    std::string m_currentNamespace;
    std::vector<FunctionNode*> m_loadFunctions;
    std::vector<FunctionNode*> m_tickFunctions;
    std::vector<std::pair<FunctionNode*, std::vector<std::string>>> m_eventFunctions;
    std::vector<std::string> m_tempEntries;
    std::vector<std::string> m_localVarEntries;
    std::unordered_map<std::string, std::string> m_scoreboardNames;
    std::unordered_map<std::string, std::string> m_storageNames;
    std::unordered_map<std::string, std::vector<std::string>> m_functionParams;
    std::unordered_map<std::string, std::vector<std::string>> m_functionParamNames;
    std::unordered_map<std::string, std::vector<std::string>> m_functionParamTypes;
    std::unordered_map<std::string, std::vector<bool>> m_functionParamIsRef;
    std::unordered_map<std::string, int> m_arraySizes;
    std::unordered_set<std::string> m_intrinsicFunctions;
    std::unordered_set<std::string> m_refIntrinsicFunctions;
    std::unordered_map<std::string, ClassDefinition> m_classDefinitions;
    std::unordered_set<std::string> m_compileTimeClasses;
    std::unordered_map<std::string, CompileTimeInstance> m_compileTimeInstances;
    std::unordered_map<std::string, std::string> m_instanceMembers; // maps "instanceName.memberName" -> scoreboard entry
    std::unordered_map<std::string, std::string> m_instanceTypes;
    int m_counter = 1;
    bool m_debugMode;

    void generateFunction(FunctionNode* function);
    std::string generateStatement(ASTNode* node);
    ExprResult generateExpression(ASTNode* node);
    std::string generateCallArgs(const std::string& funcName, const std::vector<std::unique_ptr<ASTNode>>& arguments);
    std::string generateSubFunction(const std::string& name, const std::vector<std::unique_ptr<ASTNode>>& body);
    ConditionResult generateCondition(ASTNode* node);
    void registerFunctions();
    void registerClasses();
    void generateClassMethods();
    void registerFunction(FunctionNode* fn);
    CompileTimeValue evaluateCompileTimeMethod(CompileTimeInstance& instance, const std::string& methodName, const std::vector<std::unique_ptr<ASTNode>>& arguments);
    std::string evaluateCompileTimeExpr(ASTNode* node, std::unordered_map<std::string, std::string>& locals);
    void generatePackMeta() const;
    void generateArrayGetHelper(std::string arrayName, int size) const;
    void generateTemplatePool(const std::string& poolName, const std::vector<TemplatePoolEntry>& entries) const;
    void generateStructure(const std::vector<std::string>& arguments) const;
    void generateStructureSet(const std::vector<std::string>& arguments) const;
    void generatePredicate(const std::string& predicateName, const std::string& predicateJson) const;
    void generateLootTable(const std::vector<std::string>& arguments) const;
    void generateTag(const std::vector<std::string>& arguments) const;
    void generateDimension(const std::string& dimensionName, std::string& dimensionTypeJson, std::string& dimensionGeneratorJson) const;
    void generateTickJson() const;
    void generateLoadJson() const;
    void generateEventAdvancements() const;
    void formatPrefix();
};
