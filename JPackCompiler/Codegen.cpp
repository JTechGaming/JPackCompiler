#include "Codegen.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <regex>

#include "json.hpp"

void Codegen::generate(std::string outputPath) {
    m_outputPath = std::move(outputPath);
    formatPrefix();
    registerFunctions();
    
    auto functionsPath = std::filesystem::path(m_outputPath) / "data" / m_name / "functions";
    std::filesystem::create_directories(functionsPath);
    
    generatePackMeta();

    for (auto& declaration : m_programNode->declarations) {
        if (auto* fn = dynamic_cast<FunctionNode*>(declaration.get())) {
            generateFunction(fn);
        }
    }
    
    generateTickJson();
    generateLoadJson();
}

void Codegen::generateFunction(FunctionNode* function) {
    std::string funcName = function->name;
    std::ranges::transform(funcName, funcName.begin(),
                           [](unsigned char c) { return std::tolower(c); });
    auto functionPath = std::filesystem::path(m_outputPath) / "data" / m_name / "functions" / (funcName + ".mcfunction");

    for (auto& annotation : function->annotations) {
        if (annotation->name == "tick") {
            m_tickFunctions.emplace_back(function);
        }
        if (annotation->name == "load") {
            m_loadFunctions.emplace_back(function);
        }
    }
    
    std::ofstream file(functionPath);
    
    if (function->isIntrinsic) {
        // generate macro function file
        for (auto& statement : function->body) {
            if (auto* raw = dynamic_cast<RawCommandNode*>(statement.get())) {
                std::regex pattern("\\{([^}]+)\\}");
                std::string cmd = std::regex_replace(raw->command, pattern, "$($1)");;
                file << "$" + cmd + "\n";
            }
        }
        file.close();
        return; // don't emit normal function body
    }
    
    for (size_t i = 0; i < function->parameters.size(); i++) {
        file << "scoreboard objectives add " + m_functionParams[funcName][i] + " dummy\n";
    }
    
    for (auto& statement : function->body) {
        m_tempEntries.clear();
        file << generateStatement(statement.get()) << '\n';
        for (auto& entry : m_tempEntries) {
            file << "scoreboard objectives remove " + entry + "\n";
        }
    }
    
    for (auto& entry : m_localVarEntries) {
        file << "scoreboard objectives remove " + entry + "\n";
    }
    m_localVarEntries.clear();
    
    file.close();
}

std::string Codegen::generateStatement(ASTNode* node) {
    if (auto* variable = dynamic_cast<const VariableNode*>(node)) {
        if (variable->type.name == "string") {
            std::string out;
            m_storageNames[variable->name] = variable->name;
            if (variable->value != nullptr) {
                if (auto* literal = dynamic_cast<LiteralNode*>(variable->value.get())) {
                    out = "data modify storage " + m_name + ":vars " + variable->name + " set value \"" + literal->value + "\"\n";
                } else if (auto* ident = dynamic_cast<IdentifierNode*>(variable->value.get())) {
                    out = "data modify storage " + m_name + ":vars " + variable->name + " set from storage " + m_name + ":vars " + m_storageNames[ident->name] + "\n";
                }
            }
            return out;
        }
        std::string uuid = m_prefix + makeUUID();
        m_scoreboardNames[variable->name] = uuid;
        m_localVarEntries.emplace_back(uuid);
        std::string out = "scoreboard objectives add " + uuid + " dummy\n";
        if (variable->value != nullptr) {
            ExprResult initResult = generateExpression(variable->value.get());
            out += initResult.commands;
            // copy result into the variable's entry
            out += "scoreboard players operation " + m_prefix + " " + uuid + " = " + m_prefix + " " + initResult.resultEntry + "\n";
        }
        return out;
    }
    if (auto* returnNode = dynamic_cast<ReturnNode*>(node)) {
        if (returnNode->returnVal == nullptr) {
            return "return 0\n"; // void return
        }
        ExprResult val = generateExpression(returnNode->returnVal.get());
        std::string out = val.commands;
        out += "execute store result storage " + m_name + ":return value int 1 run scoreboard players get " + m_prefix + " " + val.resultEntry + "\n";
        out += "return run data get storage " + m_name + ":return value\n";
        return out;
    }
    if (auto* ifNode = dynamic_cast<IfNode*>(node)) {
        ConditionResult condition = generateCondition(ifNode->condition.get());
        std::string ifUUID = "if_" + makeUUID().substr(2);
        std::string elseUUID = "else_" + makeUUID().substr(2);
        auto savedTemps = m_tempEntries;
        m_tempEntries.clear();
        std::string bodyFunc = generateSubFunction(ifUUID, ifNode->body);
        m_tempEntries = savedTemps;
        std::string elseBodyFunc;
        if (!ifNode->elseBody.empty()) {
            savedTemps = m_tempEntries;
            m_tempEntries.clear();
            elseBodyFunc = generateSubFunction(elseUUID, ifNode->elseBody);
            m_tempEntries = savedTemps;
        }
        std::string out = condition.commands;
        out += "execute " + condition.condition + " run function " + bodyFunc + "\n";
        if (!ifNode->elseBody.empty()) {
            out += "execute " + condition.elseCondition + " run function " + elseBodyFunc + "\n";
        }
        return out;
    }
    if (auto* whileNode = dynamic_cast<WhileNode*>(node)) {
        ConditionResult condition = generateCondition(whileNode->condition.get());
        std::string whileUUID = "while_" + makeUUID().substr(2);
        auto savedTemps = m_tempEntries;
        m_tempEntries.clear();
        std::string bodyFunc = generateSubFunction(whileUUID, whileNode->body);
        m_tempEntries = savedTemps;
        auto path = std::filesystem::path(m_outputPath) / "data" / m_name / "functions" / (whileUUID + ".mcfunction");
        std::ofstream file(path, std::ios::app); // append mode
        file << condition.commands;
        file << "execute " + condition.condition + " run function " + bodyFunc + "\n";
        file.close();
    
        std::string out = condition.commands;
        out += "execute " + condition.condition + " run function " + bodyFunc + "\n";
        return out;
    }
    if (auto* unary = dynamic_cast<UnaryExprNode*>(node)) {
        if (unary->op == TokenType::INCREMENT || unary->op == TokenType::DECREMENT) {
            if (auto* ident = dynamic_cast<IdentifierNode*>(unary->operand.get())) {
                std::string entry = m_scoreboardNames[ident->name];
                return "scoreboard players " + std::string(unary->op == TokenType::INCREMENT ? "add" : "remove") 
                       + " " + m_prefix + " " + entry + " 1\n";
            }
        }
    }
    if (auto* forNode = dynamic_cast<ForNode*>(node)) {
        std::string initializer = generateStatement(forNode->initializer.get());
        ConditionResult condition = generateCondition(forNode->condition.get());
        std::string forUUID = "for_" + makeUUID().substr(2);
        auto savedTemps = m_tempEntries;
        m_tempEntries.clear();
        std::string bodyFunc = generateSubFunction(forUUID, forNode->body);
        m_tempEntries = savedTemps;
        auto path = std::filesystem::path(m_outputPath) / "data" / m_name / "functions" / (forUUID + ".mcfunction");
        std::ofstream file(path, std::ios::app); // append mode
        file << condition.commands;
        std::cout << "increment node type: " << typeid(*forNode->increment.get()).name() << "\n";
        file << generateStatement(forNode->increment.get());
        file << "execute " + condition.condition + " run function " + bodyFunc + "\n";
        file.close();
    
        std::string out = initializer;
        out += condition.commands;
        out += "execute " + condition.condition + " run function " + bodyFunc + "\n";
        return out;
    }
    if (auto* call = dynamic_cast<CallNode*>(node)) {
        std::string funcName = call->name;
        std::ranges::transform(funcName, funcName.begin(),
            [](unsigned char c) { return std::tolower(c); });
        
        std::string out;
        auto& params = m_functionParams[funcName];
        
        if (m_intrinsicFunctions.contains(funcName)) {
            out += generateCallArgs(funcName, call->arguments);
            out += "function " + m_name + ":" + funcName + " with storage " + m_name + ":args\n";
        } else {
            out += generateCallArgs(funcName, call->arguments);
            out += "function " + m_name + ":" + funcName + "\n";
        }
        return out;
    }
    if (auto* binary = dynamic_cast<BinaryExprNode*>(node)) {
        ExprResult result = generateExpression(binary);
        return result.commands;
    }
    return "";
}

ExprResult Codegen::generateExpression(ASTNode* node) {
    if (auto* ident = dynamic_cast<IdentifierNode*>(node)) {
        if (m_storageNames.contains(ident->name)) {
            return {.commands = "", .resultEntry = m_storageNames[ident->name], .isStorage = true};
        }
        return {.commands = "", .resultEntry = m_scoreboardNames[ident->name], .isStorage = false};
    }
    if (auto* literal = dynamic_cast<LiteralNode*>(node)) {
        if (literal->type == TokenType::STRING_LITERAL) {
            return {.commands = "", .resultEntry = literal->value, .isStorage = true};
        }
        std::string tempEntry = m_prefix + makeUUID();
        m_tempEntries.emplace_back(tempEntry);
        std::string value = literal->value;
        if (literal->type == TokenType::FLOAT_LITERAL) {
            value = std::to_string(static_cast<int>(std::stof(literal->value) * 1000));
        } else if (literal->type == TokenType::DOUBLE_LITERAL) {
            value = std::to_string(static_cast<int>(std::stod(literal->value) * 1000000));
        }
        std::string commands = "scoreboard objectives add " + tempEntry + " dummy\n";
        commands += "scoreboard players set " + m_prefix + " " + tempEntry + " " + value + "\n";
        return {.commands = commands, .resultEntry = tempEntry};
    }
    if (auto* call = dynamic_cast<CallNode*>(node)) {
        std::string funcName = call->name;
        std::ranges::transform(funcName, funcName.begin(),
            [](unsigned char c) { return std::tolower(c); });
        
        std::string out;
        std::string temp = m_prefix + makeUUID();
        m_tempEntries.emplace_back(temp);
        out += "scoreboard objectives add " + temp + " dummy\n";
        out += generateCallArgs(funcName, call->arguments);
        out += "execute store result score " + m_prefix + " " + temp + " run function " + m_name + ":" + funcName + "\n";
        return {.commands = out, .resultEntry = temp};
    }
    if (auto* binary = dynamic_cast<BinaryExprNode*>(node)) {
        ExprResult leftResult  = generateExpression(binary->left.get());
        ExprResult rightResult = generateExpression(binary->right.get());
        
        std::string mcOp;
        switch (binary->op) {
            case TokenType::PLUS:   mcOp = "+="; break;
            case TokenType::MINUS:  mcOp = "-="; break;
            case TokenType::TIMES:  mcOp = "*="; break;
            case TokenType::DIVIDE: mcOp = "/="; break;
            case TokenType::MOD:    mcOp = "%="; break;
            case TokenType::EQUAL:  mcOp = "=";  break;
            default: mcOp = "+="; break;
        }
        
        std::string commands = leftResult.commands + rightResult.commands;
        if (binary->op == TokenType::EQUAL) {
            commands += "scoreboard players operation " + m_prefix + " " + leftResult.resultEntry + " = " + m_prefix + " " + rightResult.resultEntry + "\n";
            return {.commands = commands, .resultEntry = leftResult.resultEntry};
        }
        // create temp to hold result
        std::string temp = m_prefix + makeUUID();
        m_tempEntries.emplace_back(temp);
        commands += "scoreboard objectives add " + temp + " dummy\n";
        // copy left into temp
        commands += "scoreboard players operation " + m_prefix + " " + temp + " = " + m_prefix + " " + leftResult.resultEntry + "\n";
        // apply operation
        commands += "scoreboard players operation " + m_prefix + " " + temp + " " + mcOp + " " + m_prefix + " " + rightResult.resultEntry + "\n";

        return {.commands = commands, .resultEntry = temp};
    }
    
    return {.commands = "", .resultEntry = ""};
}

std::string Codegen::generateCallArgs(const std::string& funcName, const std::vector<std::unique_ptr<ASTNode>>& arguments) {
    std::string out;
    auto& params = m_functionParams[funcName];
    
    if (m_intrinsicFunctions.contains(funcName)) {
        for (int i = 0; i < arguments.size(); i++) {
            ExprResult arg = generateExpression(arguments[i].get());
            out += arg.commands;
            std::string paramName = m_functionParamNames[funcName][i];
            if (m_functionParamTypes[funcName][i] == "string") {
                if (m_storageNames.contains(arg.resultEntry)) {
                    out += "data modify storage " + m_name + ":args " + paramName + 
                           " set from storage " + m_name + ":vars " + arg.resultEntry + "\n";
                } else {
                    out += "data modify storage " + m_name + ":args " + paramName + 
                           " set value \"" + arg.resultEntry + "\"\n";
                }
            } else {
                out += "execute store result storage " + m_name + ":args " + paramName + 
                       " int 1 run scoreboard players get " + m_prefix + " " + arg.resultEntry + "\n";
            }
        }
    } else {
        for (int i = 0; i < arguments.size(); i++) {
            ExprResult arg = generateExpression(arguments[i].get());
            out += arg.commands;
            out += "scoreboard players operation " + m_prefix + " " + params[i] + 
                   " = " + m_prefix + " " + arg.resultEntry + "\n";
        }
    }
    return out;
}

std::string Codegen::generateSubFunction(const std::string& name, const std::vector<std::unique_ptr<ASTNode>>& body) {
    auto path = std::filesystem::path(m_outputPath) / "data" / m_name / ("functions/" + name + ".mcfunction");
    std::filesystem::create_directories(path.parent_path());
    std::string output;
    for (auto& statement : body) {
        m_tempEntries.clear();
        output += generateStatement(statement.get());
        for (auto& entry : m_tempEntries) {
            output += "scoreboard objectives remove " + entry + "\n";
        }
    }
    
    std::ofstream file(path);
    file << output << '\n';
    
    return m_name + ":" + name;
}

ConditionResult Codegen::generateCondition(ASTNode* node) {
    if (auto* binary = dynamic_cast<BinaryExprNode*>(node)) {
        ExprResult left = generateExpression(binary->left.get());
        ExprResult right = generateExpression(binary->right.get());

        std::string mcOp;
        bool unless = false;
        switch (binary->op) {
            case TokenType::EQUAL_EQUAL:   mcOp = " = "; break;
            case TokenType::LESSER:        mcOp = " < "; break;
            case TokenType::GREATER:       mcOp = " > "; break;
            case TokenType::LESSER_EQUAL:  mcOp = " <= "; break;
            case TokenType::GREATER_EQUAL: mcOp = " >= "; break;
            case TokenType::NOT_EQUAL:     mcOp = " = "; unless = true; break;
            default: {
                return {.commands = "", .condition = ""};
            }
        }
        
        ConditionResult result;
        result.commands = left.commands + right.commands;
        result.condition = std::string(unless ? "unless" : "if") + " score " + m_prefix + " " + left.resultEntry + mcOp + m_prefix + " " + right.resultEntry;
        result.elseCondition = std::string(unless ? "if" : "unless") + " score " + m_prefix + " " + left.resultEntry + mcOp + m_prefix + " " + right.resultEntry;
        
        return result;
    }
    return {.commands = "", .condition = ""};
}

void Codegen::registerFunctions() {
    for (auto& declaration : m_programNode->declarations) {
        if (auto* fn = dynamic_cast<FunctionNode*>(declaration.get())) {
            std::string funcName = fn->name;
            std::ranges::transform(funcName, funcName.begin(),
                [](unsigned char c) { return std::tolower(c); });
            if (fn->isIntrinsic) m_intrinsicFunctions.insert(funcName);
            for (auto& param : fn->parameters) {
                std::string uuid = m_prefix + makeUUID();
                m_functionParams[funcName].emplace_back(uuid);
                m_functionParamNames[funcName].emplace_back(param->name);
                m_functionParamTypes[funcName].emplace_back(param->type.name);
                m_scoreboardNames[param->name] = uuid;
                std::cout << "param: " << param->name << " type: " << param->type.name << "\n";
            }
        }
    }
}

void Codegen::generatePackMeta() const {
    auto path = std::filesystem::path(m_outputPath) / "pack.mcmeta";
    std::filesystem::create_directories(path.parent_path());
    nlohmann::json json;
    json["pack"]["pack_format"] = 26;
    json["pack"]["description"] = "Generated by JPackCompiler";
    
    std::ofstream file(path);
    file << std::setw(4) << json << '\n';
}

void Codegen::generateTickJson() const {
    if (m_tickFunctions.empty()) return;
    auto path = std::filesystem::path(m_outputPath) / "data/minecraft/tags/functions/tick.json";
    std::filesystem::create_directories(path.parent_path());
    nlohmann::json json;
    nlohmann::json jsonArray = nlohmann::json::array();
    for (const auto& tickFunc : m_tickFunctions) {
        std::string funcName = tickFunc->name;
        std::ranges::transform(funcName, funcName.begin(),
            [](unsigned char c) { return std::tolower(c); });
        jsonArray.push_back(m_name + ":" + funcName);
    }
    json["values"] = jsonArray;
    
    std::ofstream file(path);
    file << std::setw(4) << json << '\n';
}

void Codegen::generateLoadJson() const {
    if (m_loadFunctions.empty()) return;
    auto path = std::filesystem::path(m_outputPath) / "data/minecraft/tags/functions/load.json";
    std::filesystem::create_directories(path.parent_path());
    nlohmann::json json;
    nlohmann::json jsonArray = nlohmann::json::array();
    for (const auto& loadFunc : m_loadFunctions) {
        std::string funcName = loadFunc->name;
        std::ranges::transform(funcName, funcName.begin(),
            [](unsigned char c) { return std::tolower(c); });
        jsonArray.push_back(m_name + ":" + funcName);
    }
    json["values"] = jsonArray;
    
    std::ofstream file(path);
    file << std::setw(4) << json << '\n';
}

std::string Codegen::makeUUID() {
    return "__" + std::format("{:08X}", m_counter++);
}

void Codegen::formatPrefix() {
    m_prefix = m_name.substr(0, 6);
    std::ranges::transform(m_prefix, m_prefix.begin(),
                           [](unsigned char c) { return std::tolower(c); });
}
