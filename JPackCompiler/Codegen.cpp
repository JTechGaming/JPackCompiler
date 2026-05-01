#include "Codegen.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <regex>

#include "json.hpp"

static const std::string FUNCTIONS_DIR = "function";

void Codegen::generate(std::string outputPath) {
    m_outputPath = std::move(outputPath);
    formatPrefix();
    registerClasses();
    registerFunctions();
    
    // clean output directory before generating
    if (std::filesystem::exists(m_outputPath)) {
        std::filesystem::remove_all(m_outputPath);
    }
    
    auto functionsPath = std::filesystem::path(m_outputPath) / "data" / m_name / FUNCTIONS_DIR;
    std::filesystem::create_directories(functionsPath);
    
    generatePackMeta();
    generateClassMethods();

    for (auto& declaration : m_programNode->declarations) {
        if (auto* ns = dynamic_cast<NamespaceNode*>(declaration.get())) {
            m_currentNamespace = ns->name;
            for (auto& decl : ns->declarations) {
                if (auto* fn = dynamic_cast<FunctionNode*>(decl.get())) {
                    generateFunction(fn);
                }
            }
            m_currentNamespace = "";
        }
        if (auto* fn = dynamic_cast<FunctionNode*>(declaration.get())) {
            generateFunction(fn);
        }
    }
    
    generateTickJson();
    generateLoadJson();
    generateEventAdvancements();
}

void Codegen::generateFunction(FunctionNode* function) {
    m_localVarEntries.clear();
    auto savedScoreboardNames = m_scoreboardNames;
    auto savedStorageNames = m_storageNames;
    std::string funcName = function->name;
    std::ranges::transform(funcName, funcName.begin(),
        [](unsigned char c) { return std::tolower(c); });
    
    std::string nsLower = m_currentNamespace;
    std::ranges::transform(nsLower, nsLower.begin(),
        [](unsigned char c) { return std::tolower(c); });
    std::string funcRef = nsLower.empty() ? funcName : nsLower + "/" + funcName;
    
    auto functionPath = std::filesystem::path(m_outputPath) / "data" / m_name / FUNCTIONS_DIR / (funcRef + ".mcfunction");
    std::filesystem::create_directories(functionPath.parent_path());

    std::string dimensionName;
    std::string dimensionTypeJson;
    std::string dimensionGeneratorJson;
    
    for (auto& annotation : function->annotations) {
        if (annotation->name == "tick") {
            m_tickFunctions.emplace_back(function);
        }
        if (annotation->name == "load") {
            m_loadFunctions.emplace_back(function);
        }
        if (annotation->name == "event") {
            if (!annotation->arguments.empty()) {
                m_eventFunctions.emplace_back(function, annotation->arguments);
            }
        }
        
        if (annotation->name == "lootTable" && annotation->arguments.size() >= 2) {
            generateLootTable(annotation->arguments);
        }
        if (annotation->name == "tag" && annotation->arguments.size() >= 3) {
            generateTag(annotation->arguments);
        }
        if (annotation->name == "predicate" && annotation->arguments.size() >= 2) {
            generatePredicate(annotation->arguments[0], annotation->arguments[1]);
        }
        if (annotation->name == "dimension" && !annotation->arguments.empty()) {
            dimensionName = annotation->arguments[0];
        }
        if (annotation->name == "dimensionType" && !annotation->arguments.empty()) {
            dimensionTypeJson = annotation->arguments[0];
        }
        if (annotation->name == "dimensionGenerator" && !annotation->arguments.empty()) {
            dimensionGeneratorJson = annotation->arguments[0];
        }
        
        if (annotation->name == "templatePool" && !annotation->arguments.empty()) {
            generateTemplatePool(annotation->arguments[0], annotation->poolEntries);
        }
        if (annotation->name == "structure" && !annotation->arguments.empty()) {
            generateStructure(annotation->arguments);
        }
        if (annotation->name == "structureSet" && !annotation->arguments.empty()) {
            generateStructureSet(annotation->arguments);
        }
    }
    
    if (!dimensionName.empty()) {
        generateDimension(dimensionName, dimensionTypeJson, dimensionGeneratorJson);
        m_loadFunctions.emplace_back(function);
    }
    
    std::ofstream file(functionPath);
    
    file << "# Generated from function '" + function->name + "' [line " + 
        std::to_string(function->sourceLine) + "]\n\n";
    
    if (function->isIntrinsic && function->isStoreResultIntrinsic) {
        for (auto& statement : function->body) {
            if (auto* raw = dynamic_cast<RawCommandNode*>(statement.get())) {
                std::regex pattern("\\{([a-zA-Z_][a-zA-Z0-9_]*)\\}");
                std::string cmd = std::regex_replace(raw->command, pattern, "$($1)");
                // replace __ns placeholder with actual namespace
                std::regex nsPattern("\\$\\(__ns\\)");
                cmd = std::regex_replace(cmd, nsPattern, m_name);
                file << "$" + cmd + "\n";
                file << "return run data get storage " + m_name + ":return value\n";
            }
        }
        file.close();
        m_scoreboardNames = savedScoreboardNames;
        m_storageNames = savedStorageNames;
        return;
    }
    if (function->isIntrinsic && function->isReturnsCommand) {
        for (auto& statement : function->body) {
            if (auto* raw = dynamic_cast<RawCommandNode*>(statement.get())) {
                std::regex pattern("\\{([a-zA-Z_][a-zA-Z0-9_]*)\\}");
                std::string cmd = std::regex_replace(raw->command, pattern, "$($1)");
                file << "$execute store result storage " + m_name + ":return value int 1 run " + cmd + "\n";
                file << "return run data get storage " + m_name + ":return value\n";
            }
        }
        file.close();
        m_scoreboardNames = savedScoreboardNames;
        m_storageNames = savedStorageNames;
        return;
    }
    if (function->isIntrinsic) {
        // generate macro function file
        for (auto& statement : function->body) {
            if (auto* raw = dynamic_cast<RawCommandNode*>(statement.get())) {
                std::regex pattern("\\{([a-zA-Z_][a-zA-Z0-9_]*)\\}");
                std::string cmd = std::regex_replace(raw->command, pattern, "$($1)");
                std::regex nsPattern("\\$\\(__ns\\)");
                cmd = std::regex_replace(cmd, nsPattern, m_name);
                file << "$" + cmd + "\n";
            }
        }
        file.close();
        m_scoreboardNames = savedScoreboardNames;
        m_storageNames = savedStorageNames;
        return; // don't emit normal function body
    }
    
    for (size_t i = 0; i < function->parameters.size(); i++) {
        file << "scoreboard objectives add " + m_functionParams[funcRef][i] + " dummy\n";
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
    m_scoreboardNames = savedScoreboardNames;
    m_storageNames = savedStorageNames;
}

std::string sourceComment(const ASTNode* node) {
    if (node->sourceLine == 0) return "";
    return "# [line " + std::to_string(node->sourceLine) + "]\n";
}

std::string Codegen::generateStatement(ASTNode* node) {
    if (auto* variable = dynamic_cast<const VariableNode*>(node)) {
        if (variable->type.isArray) {
            m_storageNames[variable->name] = variable->name;
            m_arraySizes[variable->name] = variable->arraySize;
            std::string out = sourceComment(variable);
            // build NBT list from initializer
            std::string list = "[";
            for (size_t i = 0; i < variable->arrayInitializer.size(); i++) {
                if (auto* lit = dynamic_cast<LiteralNode*>(variable->arrayInitializer[i].get())) {
                    list += lit->value;
                }
                if (i < variable->arrayInitializer.size() - 1) list += ",";
            }
            list += "]";
            out += "data modify storage " + m_name + ":vars " + variable->name + " set value " + list + "\n";
            generateArrayGetHelper(variable->name, variable->arraySize);
            return out;
        }
        std::string variableTypeName = variable->type.name;
        if (m_classDefinitions.contains(variableTypeName)) {
            // instantiate class
            m_instanceTypes[variable->name] = variableTypeName;
            
            // if compile-time class, initialize instance with default member values
            if (m_compileTimeClasses.contains(variableTypeName)) {
                CompileTimeInstance instance;
                instance.className = variableTypeName;
                auto& def = m_classDefinitions[variableTypeName];
                for (auto* memberVar : def.memberVariables) {
                    std::string defaultVal;
                    if (memberVar->value != nullptr) {
                        if (auto* lit = dynamic_cast<LiteralNode*>(memberVar->value.get())) {
                            defaultVal = lit->value;
                        }
                    }
                    instance.memberValues[memberVar->name] = defaultVal;
                }
                m_compileTimeInstances[variable->name] = std::move(instance);
                return sourceComment(variable);
            }
            
            auto& def = m_classDefinitions[variableTypeName];
            std::string out = sourceComment(variable);
            // create scoreboard entries for each member variable
            for (auto* memberVar : def.memberVariables) {
                std::string entry = m_prefix + "__" + variable->name + "_" + memberVar->name + "_" + std::format("{:04x}", m_counter++);
                m_instanceMembers[variable->name + "." + memberVar->name] = entry;
                m_localVarEntries.emplace_back(entry);
                out += "scoreboard objectives add " + entry + " dummy\n";
                if (memberVar->value != nullptr) {
                    if (auto* lit = dynamic_cast<LiteralNode*>(memberVar->value.get())) {
                        out += "scoreboard players set " + m_prefix + " " + entry + " " + lit->value + "\n";
                    }
                }
            }
            return out;
        }
        std::ranges::transform(variableTypeName, variableTypeName.begin(),
            [](unsigned char c) { return std::tolower(c); });
        if (variableTypeName == "string") {
            std::string out = sourceComment(variable);
            m_storageNames[variable->name] = variable->name;
            if (variable->value != nullptr) {
                if (auto* literal = dynamic_cast<LiteralNode*>(variable->value.get())) {
                    out += "data modify storage " + m_name + ":vars " + variable->name + " set value \"" + literal->value + "\"\n";
                } else if (auto* ident = dynamic_cast<IdentifierNode*>(variable->value.get())) {
                    out += "data modify storage " + m_name + ":vars " + variable->name + " set from storage " + m_name + ":vars " + m_storageNames[ident->name] + "\n";
                } else {
                    // general case
                    ExprResult val = generateExpression(variable->value.get());
                    out += val.commands;
                    out += "data modify storage " + m_name + ":vars " + variable->name + " set value '" + val.resultEntry + "'\n";
                }
            }
            return out;
        }
        std::string uuid = m_prefix + "__" + variable->name + "_" + std::format("{:04x}", m_counter++);
        m_scoreboardNames[variable->name] = uuid;
        m_localVarEntries.emplace_back(uuid);
        std::string out = sourceComment(variable);
        out += "scoreboard objectives add " + uuid + " dummy\n";
        if (variable->value != nullptr) {
            ExprResult initResult = generateExpression(variable->value.get());
            out += initResult.commands;
            // copy result into the variable's entry
            out += "scoreboard players operation " + m_prefix + " " + uuid + " = " + m_prefix + " " + initResult.resultEntry + "\n";
        }
        return out;
    }
    if (auto* method = dynamic_cast<MethodCallNode*>(node)) {
        ExprResult result = generateExpression(method);
        std::string out = sourceComment(method);
        out += result.commands;
        return out;
    }
    if (auto* assign = dynamic_cast<MemberAssignNode*>(node)) {
        if (auto* ident = dynamic_cast<IdentifierNode*>(assign->object.get())) {
            std::string key = ident->name + "." + assign->memberName;
            if (m_instanceMembers.contains(key)) {
                ExprResult val = generateExpression(assign->value.get());
                std::string out = val.commands;
                out += "scoreboard players operation " + m_prefix + " " + m_instanceMembers[key] + " = " + m_prefix + " " + val.resultEntry + "\n";
                return out;
            }
        }
        std::cerr << "Codegen WARNING: unknown member assignment\n";
        return "";
    }
    if (auto* assign = dynamic_cast<ArrayAssignNode*>(node)) {
        ExprResult index = generateExpression(assign->index.get());
        ExprResult value = generateExpression(assign->value.get());
        std::string out = index.commands + value.commands;
        // store index and value into args, call a macro helper
        out += "execute store result storage " + m_name + ":args index int 1 run scoreboard players get " + m_prefix + " " + index.resultEntry + "\n";
        out += "execute store result storage " + m_name + ":args value int 1 run scoreboard players get " + m_prefix + " " + value.resultEntry + "\n";
        out += "data modify storage " + m_name + ":args array set from storage " + m_name + ":vars " + assign->name + "\n";
        out += "function " + m_name + ":__array_set with storage " + m_name + ":args\n";
        return out;
    }
    if (auto* returnNode = dynamic_cast<ReturnNode*>(node)) {
        if (returnNode->returnVal == nullptr) {
            return "return 0\n"; // void return
        }
        ExprResult val = generateExpression(returnNode->returnVal.get());
        std::string out = sourceComment(returnNode);
        out += val.commands;
        out += "execute store result storage " + m_name + ":return value int 1 run scoreboard players get " + m_prefix + " " + val.resultEntry + "\n";
        out += "return run data get storage " + m_name + ":return value\n";
        return out;
    }
    if (auto* ifNode = dynamic_cast<IfNode*>(node)) {
        ConditionResult condition = generateCondition(ifNode->condition.get());
        std::string ifUUID = "if_" + std::format("{:04x}", m_counter++);
        std::string elseUUID = "else_" + std::format("{:04x}", m_counter++);
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
        std::string out = "# if statement [line " + std::to_string(ifNode->sourceLine) + "]\n";
        out += condition.commands;
        out += "execute " + condition.condition + " run function " + bodyFunc + "\n";
        if (!ifNode->elseBody.empty()) {
            out += "execute " + condition.elseCondition + " run function " + elseBodyFunc + "\n";
        }
        return out;
    }
    if (auto* whileNode = dynamic_cast<WhileNode*>(node)) {
        ConditionResult condition = generateCondition(whileNode->condition.get());
        std::string whileUUID = "while_" + std::format("{:04x}", m_counter++);
        auto savedTemps = m_tempEntries;
        m_tempEntries.clear();
        std::string bodyFunc = generateSubFunction(whileUUID, whileNode->body);
        m_tempEntries = savedTemps;
        auto path = std::filesystem::path(m_outputPath) / "data" / m_name / FUNCTIONS_DIR / (whileUUID + ".mcfunction");
        std::ofstream file(path, std::ios::app); // append mode
        file << condition.commands;
        file << "execute " + condition.condition + " run function " + bodyFunc + "\n";
        file.close();
    
        std::string out = "# while statement [line " + std::to_string(whileNode->sourceLine) + "]\n";
        out += condition.commands;
        out += "execute " + condition.condition + " run function " + bodyFunc + "\n";
        return out;
    }
    if (auto* unary = dynamic_cast<UnaryExprNode*>(node)) {
        if (unary->op == TokenType::INCREMENT || unary->op == TokenType::DECREMENT) {
            if (auto* ident = dynamic_cast<IdentifierNode*>(unary->operand.get())) {
                std::string entry = m_scoreboardNames[ident->name];
                std::string out = sourceComment(ident);
                out += "scoreboard players " + std::string(unary->op == TokenType::INCREMENT ? "add" : "remove") 
                       + " " + m_prefix + " " + entry + " 1\n";
                return out;
            }
        }
    }
    if (auto* forNode = dynamic_cast<ForNode*>(node)) {
        std::string initializer = generateStatement(forNode->initializer.get());
        ConditionResult condition = generateCondition(forNode->condition.get());
        std::string forUUID = "for_" + std::format("{:04x}", m_counter++);
        auto savedTemps = m_tempEntries;
        m_tempEntries.clear();
        std::string bodyFunc = generateSubFunction(forUUID, forNode->body);
        m_tempEntries = savedTemps;
        auto path = std::filesystem::path(m_outputPath) / "data" / m_name / FUNCTIONS_DIR / (forUUID + ".mcfunction");
        std::ofstream file(path, std::ios::app); // append mode
        file << condition.commands;
        file << generateStatement(forNode->increment.get());
        file << "execute " + condition.condition + " run function " + bodyFunc + "\n";
        file.close();
    
        std::string out = "# for statement [line " + std::to_string(forNode->sourceLine) + "]\n";
        out += initializer;
        out += condition.commands;
        out += "execute " + condition.condition + " run function " + bodyFunc + "\n";
        return out;
    }
    if (auto* call = dynamic_cast<CallNode*>(node)) {
        std::string funcName = call->name;
        std::ranges::transform(funcName, funcName.begin(),
            [](unsigned char c) { return std::tolower(c); });
        
        std::string namespaceName = call->namespaceName;
        std::ranges::transform(namespaceName, namespaceName.begin(),
            [](unsigned char c) { return std::tolower(c); });
        std::string funcRef = call->namespaceName.empty() ? funcName : (namespaceName + "/" + funcName);
        
        std::string out = sourceComment(call);
        
        if (m_intrinsicFunctions.contains(funcRef)) {
            out += generateCallArgs(funcRef, call->arguments);
            out += "function " + m_name + ":" + funcRef + " with storage " + m_name + ":args\n";
        } else {
            out += generateCallArgs(funcRef, call->arguments);
            out += "function " + m_name + ":" + funcRef + "\n";
        }
        return out;
    }
    if (auto* binary = dynamic_cast<BinaryExprNode*>(node)) {
        ExprResult result = generateExpression(binary);
        std::string out = sourceComment(binary);
        out += result.commands;
        return out;
    }
    return "";
}

ExprResult Codegen::generateExpression(ASTNode* node) {
    if (auto* ident = dynamic_cast<IdentifierNode*>(node)) {
        if (m_compileTimeInstances.contains(ident->name)) {
            return {.commands = "", .resultEntry = ident->name, .isStorage = true};
        }
        if (m_storageNames.contains(ident->name)) {
            return {.commands = "", .resultEntry = m_storageNames[ident->name], .isStorage = true};
        }
        std::string entry = m_scoreboardNames[ident->name];
        if (entry.empty()) {
            std::cerr << "Codegen WARNING: undefined variable '" << ident->name 
                      << "' at line " << ident->sourceLine << "\n";
        }
        return {.commands = "", .resultEntry = entry, .isStorage = false};
    }
    if (auto* access = dynamic_cast<MemberAccessNode*>(node)) {
        if (auto* ident = dynamic_cast<IdentifierNode*>(access->object.get())) {
            std::string key = ident->name + "." + access->memberName;
            if (m_instanceMembers.contains(key)) {
                return {.commands = "", .resultEntry = m_instanceMembers[key], .isStorage = false};
            }
        }
        std::cerr << "Codegen WARNING: unknown member access\n";
        return {.commands = "", .resultEntry = ""};
    }
    if (auto* method = dynamic_cast<MethodCallNode*>(node)) {
        ExprResult objResult = generateExpression(method->object.get());
        std::string instanceName = objResult.resultEntry;
        std::string classType = m_instanceTypes[instanceName];
        if (m_compileTimeClasses.contains(classType)) {
            auto& instance = m_compileTimeInstances[instanceName];
            
            std::string methodLower = method->methodName;
            std::ranges::transform(methodLower, methodLower.begin(),
                [](unsigned char c) { return std::tolower(c); });
            
            CompileTimeValue result = evaluateCompileTimeMethod(
                instance, methodLower, method->arguments);
            
            if (result.isThis) {
                // return the instance itself for chaining
                // represent as a special storage value
                return {.commands = "", .resultEntry = instanceName, .isStorage = true};
            }
            return {.commands = "", .resultEntry = result.stringValue, .isStorage = true};
        }
        
        if (auto* ident = dynamic_cast<IdentifierNode*>(method->object.get())) {
            // find which class this instance belongs to
            std::string instanceName = ident->name;
            std::string classType = m_instanceTypes[instanceName];
            std::string classNameLower = classType;
            std::ranges::transform(classNameLower, classNameLower.begin(),
                [](unsigned char c) { return std::tolower(c); });
            std::string methodNameLower = method->methodName;
            std::ranges::transform(methodNameLower, methodNameLower.begin(),
                [](unsigned char c) { return std::tolower(c); });
            std::string methodRef = classNameLower + "/" + methodNameLower;
        
            std::string out;
            std::string temp = m_prefix + "__tmp_" + std::format("{:04x}", m_counter++);
            m_tempEntries.emplace_back(temp);
            out += "scoreboard objectives add " + temp + " dummy\n";
        
            // pass member variables as implicit arguments
            auto& classDef = m_classDefinitions[classType];
            for (size_t i = 0; i < classDef.memberVariables.size(); i++) {
                std::string key = instanceName + "." + classDef.memberVariables[i]->name;
                std::string entry = m_instanceMembers[key];
                out += "scoreboard players operation " + m_prefix + " " + 
                       m_functionParams[methodRef][i] + 
                       " = " + m_prefix + " " + entry + "\n";
            }
        
            // pass explicit arguments
            out += generateCallArgs(methodRef, method->arguments);
            out += "execute store result score " + m_prefix + " " + temp + 
                   " run function " + m_name + ":" + methodRef + "\n";
            return {.commands = out, .resultEntry = temp};
        }
        return {.commands = "", .resultEntry = ""};
    }
    if (auto* access = dynamic_cast<ArrayAccessNode*>(node)) {
        ExprResult index = generateExpression(access->index.get());
        std::string temp = m_prefix + "__tmp_" + std::format("{:04x}", m_counter++);
        m_tempEntries.emplace_back(temp);
        std::string out = index.commands;
        out += "scoreboard objectives add " + temp + " dummy\n";
        out += "execute store result storage " + m_name + ":args index int 1 run scoreboard players get " + m_prefix + " " + index.resultEntry + "\n";
        out += "data modify storage " + m_name + ":args array set from storage " + m_name + ":vars " + access->name + "\n";
        std::string arrayNameLower = access->name;
        std::ranges::transform(arrayNameLower, arrayNameLower.begin(),
            [](unsigned char c) { return std::tolower(c); });
        out += "execute store result score " + m_prefix + " " + temp + " run function " + m_name + ":__array_get_" + arrayNameLower + " with storage " + m_name + ":args\n";
        return {.commands = out, .resultEntry = temp};
    }
    if (auto* literal = dynamic_cast<LiteralNode*>(node)) {
        if (literal->type == TokenType::STRING_LITERAL) {
            return {.commands = "", .resultEntry = literal->value, .isStorage = true};
        }
        std::string tempEntry = m_prefix + "__tmp_" + std::format("{:04x}", m_counter++);
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
    if (auto* json = dynamic_cast<JsonNode*>(node)) {
        return {.commands = "", .resultEntry = serializeJson(json), .isStorage = true};
    }
    if (auto* call = dynamic_cast<CallNode*>(node)) {
        std::string funcName = call->name;
        std::ranges::transform(funcName, funcName.begin(),
            [](unsigned char c) { return std::tolower(c); });
    
        std::string namespaceName = call->namespaceName;
        std::ranges::transform(namespaceName, namespaceName.begin(),
            [](unsigned char c) { return std::tolower(c); });
        std::string funcRef = call->namespaceName.empty() ? funcName : (namespaceName + "/" + funcName);
    
        std::string out;
        std::string temp = m_prefix + "__tmp_" + std::format("{:04x}", m_counter++);
        m_tempEntries.emplace_back(temp);
        out += "scoreboard objectives add " + temp + " dummy\n";
        out += generateCallArgs(funcRef, call->arguments);
    
        if (m_intrinsicFunctions.contains(funcRef)) {
            out += "function " + m_name + ":" + funcRef + " with storage " + m_name + ":args\n";
            out += "execute store result score " + m_prefix + " " + temp + " run data get storage " + m_name + ":return value\n";
        } else {
            out += "execute store result score " + m_prefix + " " + temp + " run function " + m_name + ":" + funcRef + "\n";
        }
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
        std::string temp = m_prefix + "__tmp_" + std::format("{:04x}", m_counter++);
        m_tempEntries.emplace_back(temp);
        commands += "scoreboard objectives add " + temp + " dummy\n";
        // copy left into temp
        commands += "scoreboard players operation " + m_prefix + " " + temp + " = " + m_prefix + " " + leftResult.resultEntry + "\n";
        // apply operation
        commands += "scoreboard players operation " + m_prefix + " " + temp + " " + mcOp + " " + m_prefix + " " + rightResult.resultEntry + "\n";

        return {.commands = commands, .resultEntry = temp};
    }
    if (auto* unary = dynamic_cast<UnaryExprNode*>(node)) {
        if (unary->op == TokenType::MINUS) {
            ExprResult operand = generateExpression(unary->operand.get());
            std::string temp = m_prefix + "__tmp_" + std::format("{:04x}", m_counter++);
            m_tempEntries.emplace_back(temp);
            std::string commands = operand.commands;
            commands += "scoreboard objectives add " + temp + " dummy\n";
            // negate by multiplying by -1
            std::string negEntry = m_prefix + "__tmp_" + std::format("{:04x}", m_counter++);
            m_tempEntries.emplace_back(negEntry);
            commands += "scoreboard objectives add " + negEntry + " dummy\n";
            commands += "scoreboard players set " + m_prefix + " " + negEntry + " -1\n";
            commands += "scoreboard players operation " + m_prefix + " " + temp + " = " + m_prefix + " " + operand.resultEntry + "\n";
            commands += "scoreboard players operation " + m_prefix + " " + temp + " *= " + m_prefix + " " + negEntry + "\n";
            return {.commands = commands, .resultEntry = temp};
        }
    }
    
    return {.commands = "", .resultEntry = ""};
}

std::string Codegen::generateCallArgs(const std::string& funcName, const std::vector<std::unique_ptr<ASTNode>>& arguments) {
    std::string out;
    auto& params = m_functionParams[funcName];
    
    if (!m_intrinsicFunctions.contains(funcName) && m_functionParams[funcName].empty()) {
        std::cerr << "Codegen WARNING: undefined function '" << funcName << "'\n";
    }
    
    if (m_intrinsicFunctions.contains(funcName)) {
        for (int i = 0; i < arguments.size(); i++) {
            std::string paramName = m_functionParamNames[funcName][i];
            if (m_functionParamIsRef[funcName][i]) {
                // reference parameter so pass scoreboard entry name and objective
                ExprResult arg = generateExpression(arguments[i].get());
                out += arg.commands;
                out += "data modify storage " + m_name + ":args " + paramName + "_player" +
                       " set value '" + m_prefix + "'\n";
                out += "data modify storage " + m_name + ":args " + paramName + "_obj" +
                       " set value '" + arg.resultEntry + "'\n";
                continue;
            }
            if (m_functionParamTypes[funcName][i] == "string") {
                if (auto* callArg = dynamic_cast<CallNode*>(arguments[i].get())) {
                    // extract function path at compile time, don't evaluate
                    std::string argFuncName = callArg->name;
                    std::string argNs = callArg->namespaceName;
                    std::ranges::transform(argFuncName, argFuncName.begin(),
                        [](unsigned char c) { return std::tolower(c); });
                    std::ranges::transform(argNs, argNs.begin(),
                        [](unsigned char c) { return std::tolower(c); });
                    std::string funcPath = argNs.empty() ? argFuncName : argNs + "/" + argFuncName;
                    out += "data modify storage " + m_name + ":args " + paramName +
                           " set value '" + m_name + ":" + funcPath + "'\n";
                    continue; // skip the generateExpression call entirely
                }
            }
            ExprResult arg = generateExpression(arguments[i].get());
            out += arg.commands;
            if (m_functionParamTypes[funcName][i] == "float") {
                out += "execute store result storage " + m_name + ":args " + paramName + 
                       " float 0.001 run scoreboard players get " + m_prefix + " " + arg.resultEntry + "\n";
            } else if (m_functionParamTypes[funcName][i] == "double") {
                out += "execute store result storage " + m_name + ":args " + paramName + 
                       " double 0.000001 run scoreboard players get " + m_prefix + " " + arg.resultEntry + "\n";
            } else if (m_functionParamTypes[funcName][i] == "string") {
                if (m_storageNames.contains(arg.resultEntry)) {
                    out += "data modify storage " + m_name + ":args " + paramName +
                           " set from storage " + m_name + ":vars " + arg.resultEntry + "\n";
                } else {
                    out += "data modify storage " + m_name + ":args " + paramName + 
                           " set value '" + arg.resultEntry + "'\n";
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
    auto savedScoreboardNames = m_scoreboardNames;
    auto savedStorageNames = m_storageNames;
    auto savedLocalVars = m_localVarEntries;
    m_localVarEntries.clear();
    
    auto path = std::filesystem::path(m_outputPath) / "data" / m_name / (FUNCTIONS_DIR + "/" + name + ".mcfunction");
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
    
    m_scoreboardNames = savedScoreboardNames;
    m_storageNames = savedStorageNames;
    m_localVarEntries = savedLocalVars;
    return m_name + ":" + name;
}

ConditionResult Codegen::generateCondition(ASTNode* node) {
    if (auto* matches = dynamic_cast<MatchesExprNode*>(node)) {
        ExprResult target = generateExpression(matches->target.get());
        std::string range;
        if (matches->hasMin && matches->hasRange && matches->hasMax) {
            range = std::to_string(matches->min) + ".." + std::to_string(matches->max);
        } else if (matches->hasMin && matches->hasRange) {
            range = std::to_string(matches->min) + "..";
        } else if (matches->hasRange && matches->hasMax) {
            range = ".." + std::to_string(matches->max);
        } else {
            range = std::to_string(matches->min);
        }
    
        ConditionResult result;
        result.commands = target.commands;
        result.condition = "if score " + m_prefix + " " + target.resultEntry + " matches " + range;
        result.elseCondition = "unless score " + m_prefix + " " + target.resultEntry + " matches " + range;
        return result;
    }
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

std::string Codegen::serializeJson(const JsonNode* json) {
    switch (json->valueType) {
        case JsonValueType::Object: {
            std::string out = "{";
            for (size_t i = 0; i < json->objectEntries.size(); i++) {
                auto& entry = json->objectEntries[i];
                out += "\"" + entry.first + "\": " + serializeJson(entry.second.get());
                if (i < json->objectEntries.size() - 1) out += ",";
            }
            out += "}";
            return out;
        }
        case JsonValueType::Array: {
            std::string out = "[";
            for (size_t i = 0; i < json->arrayElements.size(); i++) {
                auto& entry = json->arrayElements[i];
                out += serializeJson(entry.get());
                if (i < json->arrayElements.size() - 1) out += ",";
            }
            out += "]";
            return out;
        }
        case JsonValueType::String: {
            return '"' + json->stringValue + '"';
        }
        case JsonValueType::Number: {
            double intPart;
            if (std::modf(json->numberValue, &intPart) == 0.0) {
                return std::to_string(static_cast<int>(intPart));
            }
            return std::to_string(json->numberValue);
        }
        case JsonValueType::Bool: {
            return json->boolValue ? "true" : "false";
        }
        case JsonValueType::Null: {
            return "null";
        }
    }
    return "null";
}

void Codegen::registerFunctions() {
    for (auto& declaration : m_programNode->declarations) {
        if (auto* ns = dynamic_cast<NamespaceNode*>(declaration.get())) {
            m_currentNamespace = ns->name;
            for (auto& decl : ns->declarations) {
                if (auto* fn = dynamic_cast<FunctionNode*>(decl.get())) {
                    registerFunction(fn);
                }
            }
            m_currentNamespace = "";
        }
        if (auto* fn = dynamic_cast<FunctionNode*>(declaration.get())) {
            registerFunction(fn);
        }
    }
}

void Codegen::registerClasses() {
    for (auto& declaration : m_programNode->declarations) {
        if (auto* classNode = dynamic_cast<ClassNode*>(declaration.get())) {
            ClassDefinition def;
            // collect all members from both public and private
            for (auto& member : classNode->publicMembers) {
                if (auto* var = dynamic_cast<VariableNode*>(member.get())) {
                    def.memberVariables.emplace_back(var);
                } else if (auto* fn = dynamic_cast<FunctionNode*>(member.get())) {
                    def.memberFunctions.emplace_back(fn);
                }
            }
            for (auto& member : classNode->privateMembers) {
                if (auto* var = dynamic_cast<VariableNode*>(member.get())) {
                    def.memberVariables.emplace_back(var);
                } else if (auto* fn = dynamic_cast<FunctionNode*>(member.get())) {
                    def.memberFunctions.emplace_back(fn);
                }
            }
            
            for (auto& annotation : classNode->annotations) {
                if (annotation->name == "compile_time_class") {
                    m_compileTimeClasses.insert(classNode->name);
                }
            }
            
            // register method functions with implicit member variable parameters
            std::string classNameLower = classNode->name;
            std::ranges::transform(classNameLower, classNameLower.begin(),
                [](unsigned char c) { return std::tolower(c); });
            
            for (auto* fn : def.memberFunctions) {
                std::string funcNameLower = fn->name;
                std::ranges::transform(funcNameLower, funcNameLower.begin(),
                    [](unsigned char c) { return std::tolower(c); });
                std::string methodRef = classNameLower + "/" + funcNameLower;
                
                // implicit member variable parameters
                for (auto* memberVar : def.memberVariables) {
                    std::string uuid = m_prefix + "__" + memberVar->name + "_" + std::format("{:04x}", m_counter++);
                    m_functionParams[methodRef].emplace_back(uuid);
                    m_functionParamNames[methodRef].emplace_back(memberVar->name);
                    std::string typeLower = memberVar->type.name;
                    std::ranges::transform(typeLower, typeLower.begin(),
                        [](unsigned char c) { return std::tolower(c); });
                    m_functionParamTypes[methodRef].emplace_back(typeLower);
                    m_functionParamIsRef[methodRef].emplace_back(false);
                    // map member name to this entry so method body can find it
                    m_scoreboardNames[memberVar->name] = uuid;
                }
                
                // explicit parameters
                for (auto& param : fn->parameters) {
                    std::string uuid = m_prefix + "__" + param->name + "_" + std::format("{:04x}", m_counter++);
                    m_functionParams[methodRef].emplace_back(uuid);
                    m_functionParamNames[methodRef].emplace_back(param->name);
                    std::string typeLower = param->type.name;
                    std::ranges::transform(typeLower, typeLower.begin(),
                        [](unsigned char c) { return std::tolower(c); });
                    m_functionParamTypes[methodRef].emplace_back(typeLower);
                    m_functionParamIsRef[methodRef].emplace_back(param->type.isReference);
                    m_scoreboardNames[param->name] = uuid;
                }
            }
            
            m_classDefinitions[classNode->name] = std::move(def);
        }
    }
}

void Codegen::generateClassMethods() {
    for (auto& def : m_classDefinitions) {
        if (m_compileTimeClasses.contains(def.first)) continue;
        for (auto& function : def.second.memberFunctions) {
            m_localVarEntries.clear();
            auto savedScoreboardNames = m_scoreboardNames;
            auto savedStorageNames = m_storageNames;
            std::string funcName = function->name;
            std::ranges::transform(funcName, funcName.begin(),
                                   [](unsigned char c) { return std::tolower(c); });
        
            std::string nsLower = def.first;
            std::ranges::transform(nsLower, nsLower.begin(),
                                   [](unsigned char c) { return std::tolower(c); });
            std::string funcRef = nsLower.empty() ? funcName : nsLower + "/" + funcName;
        
            auto functionPath = std::filesystem::path(m_outputPath) / "data" / m_name / FUNCTIONS_DIR / (funcRef + ".mcfunction");
            std::filesystem::create_directories(functionPath.parent_path());
        
            std::ofstream file(functionPath);
        
            file << "# Generated from class function '" + def.first + "::" + function->name + "' [line " + 
                std::to_string(function->sourceLine) + "]\n\n";
            
            for (const auto& objective : m_functionParams[funcRef]) {
                file << "scoreboard objectives add " + objective + " dummy\n";
            }
            
            // restore member variable to parameter mappings for this method
            auto& classDef = m_classDefinitions[def.first];
            for (size_t i = 0; i < classDef.memberVariables.size(); i++) {
                m_scoreboardNames[classDef.memberVariables[i]->name] = m_functionParams[funcRef][i];
            }
            // then explicit parameters
            for (size_t i = 0; i < function->parameters.size(); i++) {
                m_scoreboardNames[function->parameters[i]->name] = m_functionParams[funcRef][classDef.memberVariables.size() + i];
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
            m_scoreboardNames = savedScoreboardNames;
            m_storageNames = savedStorageNames;
        }
    }
}

void Codegen::registerFunction(FunctionNode* fn) {
    std::string funcName = fn->name;
    std::ranges::transform(funcName, funcName.begin(),
        [](unsigned char c) { return std::tolower(c); });
    
    std::string nsLower = m_currentNamespace;
    std::ranges::transform(nsLower, nsLower.begin(),
        [](unsigned char c) { return std::tolower(c); });
    
    std::string func = nsLower.empty() ? funcName : nsLower + "/" + funcName;
            
    if (fn->isIntrinsic) m_intrinsicFunctions.insert(func);
    if (fn->isRefIntrinsic) m_refIntrinsicFunctions.insert(func);
    for (auto& param : fn->parameters) {
        std::string uuid = m_prefix + "__" + param->name + "_" + std::format("{:04x}", m_counter++);
        m_functionParams[func].emplace_back(uuid);
        m_functionParamNames[func].emplace_back(param->name);
        std::string paramTypeNameLower = param->type.name;
        std::ranges::transform(paramTypeNameLower, paramTypeNameLower.begin(),
            [](unsigned char c) { return std::tolower(c); });
        m_functionParamTypes[func].emplace_back(paramTypeNameLower);
        m_functionParamIsRef[func].emplace_back(param->type.isReference);
        m_scoreboardNames[param->name] = uuid;
    }
}

CompileTimeValue Codegen::evaluateCompileTimeMethod(CompileTimeInstance& instance, const std::string& methodName,
                                                    const std::vector<std::unique_ptr<ASTNode>>& arguments) {
    
    auto& classDef = m_classDefinitions[instance.className];
    
    // find the method
    FunctionNode* method = nullptr;
    for (auto* fn : classDef.memberFunctions) {
        std::string fnLower = fn->name;
        std::ranges::transform(fnLower, fnLower.begin(),
            [](unsigned char c) { return std::tolower(c); });
        if (fnLower == methodName) {
            method = fn;
            break;
        }
    }
    if (!method) return {.stringValue = "", .isThis = false};
    
    // set up parameter values
    std::unordered_map<std::string, std::string> localStrings = instance.memberValues;
    for (size_t i = 0; i < method->parameters.size() && i < arguments.size(); i++) {
        std::string argVal = evaluateCompileTimeExpr(arguments[i].get(), localStrings);
        localStrings[method->parameters[i]->name] = argVal;
    }
    
    // evaluate method body
    for (auto& statement : method->body) {
        if (auto* ret = dynamic_cast<ReturnNode*>(statement.get())) {
            if (auto* thisNode = dynamic_cast<ThisNode*>(ret->returnVal.get())) {
                // update instance member values and return this
                instance.memberValues = localStrings;
                return {.stringValue = "", .isThis = true};
            }
            // return a string value
            std::string val = evaluateCompileTimeExpr(ret->returnVal.get(), localStrings);
            // strip trailing comma before ] if present
            if (val.size() >= 2 && val[val.size()-2] == ',') {
                val.erase(val.size()-2, 1);
            }
            return {.stringValue = val, .isThis = false};
        }
        if (auto* assign = dynamic_cast<MemberAssignNode*>(statement.get())) {
            std::string val = evaluateCompileTimeExpr(assign->value.get(), localStrings);
            localStrings[assign->memberName] = val;
        }
        if (auto* binary = dynamic_cast<BinaryExprNode*>(statement.get())) {
            if (binary->op == TokenType::EQUAL) {
                if (auto* ident = dynamic_cast<IdentifierNode*>(binary->left.get())) {
                    std::string val = evaluateCompileTimeExpr(binary->right.get(), localStrings);
                    localStrings[ident->name] = val;
                }
            }
        }
        if (auto* var = dynamic_cast<VariableNode*>(statement.get())) {
            std::string val = var->value ? evaluateCompileTimeExpr(var->value.get(), localStrings) : "";
            localStrings[var->name] = val;
        }
    }
    instance.memberValues = localStrings;
    return {.stringValue = "", .isThis = false};
}

std::string Codegen::evaluateCompileTimeExpr(ASTNode* node, std::unordered_map<std::string, std::string>& locals) {
    if (auto* lit = dynamic_cast<LiteralNode*>(node)) {
        if (lit->type == TokenType::INT_LITERAL) {
            return lit->value; // may need special handlign in the future
        }
        return lit->value;
    }
    if (auto* ident = dynamic_cast<IdentifierNode*>(node)) {
        if (locals.contains(ident->name)) return locals[ident->name];
        if (m_scoreboardNames.contains(ident->name)) {
            std::cerr << "Codegen WARNING: cannot use runtime value '" 
                      << ident->name << "' in compile-time context\n";
        }
        return "";
    }
    if (auto* member = dynamic_cast<MemberAccessNode*>(node)) {
        if (auto* ident = dynamic_cast<IdentifierNode*>(member->object.get())) {
            if (locals.contains(ident->name + "." + member->memberName))
                return locals[ident->name + "." + member->memberName];
        }
        return "";
    }
    if (auto* binary = dynamic_cast<BinaryExprNode*>(node)) {
        if (binary->op == TokenType::PLUS) {
            return evaluateCompileTimeExpr(binary->left.get(), locals) +
                   evaluateCompileTimeExpr(binary->right.get(), locals);
        }
    }
    if (auto* json = dynamic_cast<JsonNode*>(node)) {
        return serializeJson(json);
    }
    return "";
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

void Codegen::generateArrayGetHelper(std::string arrayName, int size) const {
    std::string arrayNameLower = arrayName;
    std::ranges::transform(arrayNameLower, arrayNameLower.begin(),
        [](unsigned char c) { return std::tolower(c); });
    
    auto getPath = std::filesystem::path(m_outputPath) / "data" / m_name / FUNCTIONS_DIR / ("__array_get_" + arrayNameLower + ".mcfunction");
    std::ofstream getFile(getPath);
    
    getFile << "scoreboard objectives add " + m_prefix + "__cmp dummy\n";
    getFile << "scoreboard objectives add " + m_prefix + "__idx_arg dummy\n";
    for (int i = 0; i < size; ++i) {
        getFile << "scoreboard players set " + m_prefix + " " + m_prefix + "__cmp " + std::to_string(i) + "\n";
        getFile << "execute store result score " + m_prefix + " " + m_prefix + "__idx_arg run data get storage " + m_name + ":args index\n";
        getFile << "execute if score " + m_prefix + " " + m_prefix + "__idx_arg = " + m_prefix + " " + m_prefix + "__cmp run return run data get storage " + m_name + ":vars " + arrayName + "[" + std::to_string(i) + "]\n";
    }
    getFile << "scoreboard objectives remove " + m_prefix + "__cmp\n";
    getFile << "scoreboard objectives remove " + m_prefix + "__idx_arg\n";
}

void Codegen::generateTemplatePool(const std::string& poolName, const std::vector<TemplatePoolEntry>& entries) const {
    std::string namespacePart = poolName;
    std::string pathPart = poolName;
    size_t colon = poolName.find(':');
    if (colon != std::string::npos) {
        namespacePart = poolName.substr(0, colon);
        pathPart = poolName.substr(colon + 1);
    }
    
    auto path = std::filesystem::path(m_outputPath) / "data" / namespacePart / "worldgen" / "template_pool" / (pathPart + ".json");
    std::filesystem::create_directories(path.parent_path());
    
    nlohmann::json json;
    json["fallback"] = "minecraft:empty";
    json["elements"] = nlohmann::json::array();
    
    for (const auto& entry : entries) {
        nlohmann::json element;
        element["weight"] = entry.weight;
        element["element"]["element_type"] = "minecraft:single_pool_element";
        element["element"]["location"] = entry.location;
        element["element"]["projection"] = "rigid";
        element["element"]["processors"] = "minecraft:empty";
        json["elements"].push_back(element);
    }
    
    std::ofstream file(path);
    file << std::setw(4) << json << '\n';
}

void Codegen::generateStructure(const std::vector<std::string>& arguments) const {
    const std::string& structureName = arguments[0];
    std::string startPool = arguments[1];
    std::string startJigsawName = arguments[2];
    int size = std::stoi(arguments[3]);
    std::string namespacePart = structureName;
    std::string pathPart = structureName;
    size_t colon = structureName.find(':');
    if (colon != std::string::npos) {
        namespacePart = structureName.substr(0, colon);
        pathPart = structureName.substr(colon + 1);
    }
    
    auto path = std::filesystem::path(m_outputPath) / "data" / namespacePart / "worldgen" / "structure" / (pathPart + ".json");
    std::filesystem::create_directories(path.parent_path());
    
    nlohmann::json json;
    json["type"] = "minecraft:jigsaw";
    json["start_pool"] = startPool;
    json["size"] = size;
    json["max_distance_from_center"] = 80;
    json["use_expansion_hack"] = false;
    json["start_jigsaw_name"] = startJigsawName;
    json["step"] = "surface_structures";
    json["spawn_overrides"] = nlohmann::json::object();
    json["start_height"]["absolute"] = 0;
    json["biomes"] = "#minecraft:is_overworld";
    
    std::ofstream file(path);
    file << std::setw(4) << json << '\n';
}

void Codegen::generateStructureSet(const std::vector<std::string>& arguments) const {
    const std::string& setName = arguments[0];
    int spacing = std::stoi(arguments[1]);
    int separation = std::stoi(arguments[2]);
    
    std::string namespacePart = setName;
    std::string pathPart = setName;
    size_t colon = setName.find(':');
    if (colon != std::string::npos) {
        namespacePart = setName.substr(0, colon);
        pathPart = setName.substr(colon + 1);
    }
    
    auto path = std::filesystem::path(m_outputPath) / "data" / namespacePart / "worldgen" / "structure_set" / (pathPart + ".json");
    std::filesystem::create_directories(path.parent_path());
    
    nlohmann::json json;
    json["structures"] = nlohmann::json::array();
    nlohmann::json entry;
    entry["structure"] = setName;
    entry["weight"] = 1;
    json["structures"].push_back(entry);
    json["placement"]["type"] = "minecraft:random_spread";
    json["placement"]["spacing"] = spacing;
    json["placement"]["separation"] = separation;
    json["placement"]["salt"] = 12345;
    
    std::ofstream file(path);
    file << std::setw(4) << json << '\n';
}

void Codegen::generatePredicate(const std::string& predicateName, const std::string& predicateJson) const {
    std::string namespacePart = predicateName;
    std::string pathPart = predicateName;
    size_t colon = predicateName.find(':');
    if (colon != std::string::npos) {
        namespacePart = predicateName.substr(0, colon);
        pathPart = predicateName.substr(colon + 1);
    }
    
    auto path = std::filesystem::path(m_outputPath) / "data" / namespacePart / "predicate" / (pathPart + ".json");
    std::filesystem::create_directories(path.parent_path());
    
    nlohmann::json json = nlohmann::json::parse(predicateJson);
    std::ofstream file(path);
    file << std::setw(4) << json << '\n';
}

void Codegen::generateLootTable(const std::vector<std::string>& arguments) const {
    const std::string& tableName = arguments[0];
    const std::string& tableJson = arguments[1];
    
    auto path = std::filesystem::path(m_outputPath) / "data" / m_name / "loot_table" / (tableName + ".json");
    std::filesystem::create_directories(path.parent_path());
    
    nlohmann::json json = nlohmann::json::parse(tableJson);
    std::ofstream file(path);
    file << std::setw(4) << json << '\n';
}

void Codegen::generateTag(const std::vector<std::string>& arguments) const {
    const std::string& tagType = arguments[0];
    const std::string& tagName = arguments[1];
    const std::string& values = arguments[2];// serialized JSON array
    
    auto path = std::filesystem::path(m_outputPath) / "data" / m_name / "tags" / tagType / (tagName + ".json");
    std::filesystem::create_directories(path.parent_path());
    
    nlohmann::json json;
    json["values"] = nlohmann::json::parse(values);
    
    std::ofstream file(path);
    file << std::setw(4) << json << '\n';
}

void Codegen::generateDimension(const std::string& dimensionName, std::string& dimensionTypeJson, std::string& dimensionGeneratorJson) const {
    auto typePath = std::filesystem::path(m_outputPath) / "data" / m_name / "dimension_type" / (dimensionName + ".json");
    std::filesystem::create_directories(typePath.parent_path());
    auto defPath = std::filesystem::path(m_outputPath) / "data" / m_name / "dimension" / (dimensionName + ".json");
    std::filesystem::create_directories(defPath.parent_path());
    
    if (dimensionTypeJson.empty()) {
        dimensionTypeJson = "{"
            "\"ultrawarm\": false,"
            "\"natural\": true,"
            "\"has_skylight\": true,"
            "\"has_ceiling\": false,"
            "\"ambient_light\": 0.0,"
            "\"bed_works\": false,"
            "\"respawn_anchor_works\": false,"
            "\"has_raids\": true,"
            "\"piglin_safe\": false,"
            "\"monster_spawn_block_light_limit\": 0,"
            "\"monster_spawn_light_level\": 7,"
            "\"height\": 256,"
            "\"min_y\": 0,"
            "\"logical_height\": 256,"
            "\"coordinate_scale\": 1.0,"
            "\"infiniburn\": \"#minecraft:infiniburn_overworld\","
            "\"effects\": \"minecraft:overworld\""
        "}";
    }
    if (dimensionGeneratorJson.empty()) {
        dimensionGeneratorJson = "{"
            "\"type\": \"" + m_name + ":" + dimensionName + "\","
            "\"generator\": {\"type\": \"minecraft:flat\","
            "\"settings\": {\"layers\": [{\"block\": \"minecraft:air\", \"height\": 1}],"
            "\"biome\": \"minecraft:the_void\"}}}";
    }
    
    nlohmann::json typeJson = nlohmann::json::parse(dimensionTypeJson);
    std::ofstream typeFile(typePath);
    typeFile << std::setw(4) << typeJson << '\n';
    typeFile.close();
    
    nlohmann::json defJson = nlohmann::json::parse(dimensionGeneratorJson);
    std::ofstream defFile(defPath);
    defFile << std::setw(4) << defJson << '\n';
    defFile.close();
}

void Codegen::generateTickJson() const {
    if (m_tickFunctions.empty()) return;
    auto path = std::filesystem::path(m_outputPath) / ("data/minecraft/tags/" + FUNCTIONS_DIR + "/tick.json");
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
    auto path = std::filesystem::path(m_outputPath) / ("data/minecraft/tags/" + FUNCTIONS_DIR + "/load.json");
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

void Codegen::generateEventAdvancements() const {
    if (m_eventFunctions.empty()) return;
    for (const auto& function : m_eventFunctions) {
        std::string funcName = function.first->name;
        std::ranges::transform(funcName, funcName.begin(),
            [](unsigned char c) { return std::tolower(c); });
        auto path = std::filesystem::path(m_outputPath) / "data" / m_name / "advancement" / "events" / (funcName + ".json");
        std::filesystem::create_directories(path.parent_path());
        nlohmann::json json;
        nlohmann::json jsonArray = nlohmann::json::array();
        
        json["criteria"]["requirement"]["trigger"] = function.second[0];
        json["rewards"]["function"] = m_name + ":" + funcName;
        if (function.second.size() > 1) {
            json["criteria"]["requirement"]["conditions"] = nlohmann::json::parse(function.second[1]);
        }
        
        std::ofstream file(path);
        file << std::setw(4) << json << '\n';
        
        if (function.first->isRevoke) {
            auto funcPath = std::filesystem::path(m_outputPath) / "data" / m_name / FUNCTIONS_DIR / (funcName + ".mcfunction");
            std::ofstream funcFile(funcPath, std::ios::app);
            funcFile << "advancement revoke @a only " + m_name + ":events/" + funcName + "\n";
        }
    }
}

void Codegen::formatPrefix() {
    std::ranges::transform(m_name, m_name.begin(),
        [](unsigned char c) { return std::tolower(c); });
    m_prefix = m_name.substr(0, 6);
}
