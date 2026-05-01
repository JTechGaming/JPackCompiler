#pragma once
#include <string>
#include "json.hpp"

struct FunctionInfo {
    std::string name;
    std::string namespaceName;
    std::string returnType;
    std::vector<std::pair<std::string, std::string>> parameters; // type, name
};

class LanguageServer {
public:
    void run();
private:
    std::vector<FunctionInfo> m_functions;
    std::string m_currentSource;
    
    void parseSource(const std::string& source);
    std::vector<FunctionInfo> getFunctionsInNamespace(const std::string& ns);
    FunctionInfo* findFunction(const std::string& ns, const std::string& name);
    
    void handleMessage(const nlohmann::json& message);
    void handleInitialize(const nlohmann::json& id, const nlohmann::json& params);
    void handleCompletion(const nlohmann::json& id, const nlohmann::json& params);
    void handleSignatureHelp(const nlohmann::json& id, const nlohmann::json& params);
    void handleHover(const nlohmann::json& id, const nlohmann::json& params);
    void sendResponse(const nlohmann::json& id, const nlohmann::json& result);
    void sendMessage(const nlohmann::json& message);
    std::string readMessage();
};