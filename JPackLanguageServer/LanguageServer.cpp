#include "LanguageServer.h"

#include <fstream>
#include <iostream>

#include "AST.h"
#include "Lexer.h"
#include "Parser.h"

void LanguageServer::run() {
    while (true) {
        std::string message = readMessage();
        if (message.empty()) break;
        auto json = nlohmann::json::parse(message);
        handleMessage(json);
    }
}

void LanguageServer::parseSource(const std::string& source) {
    m_functions.clear();
    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        if (!program) return;

        for (auto& decl : program->declarations) {
            if (auto* ns = dynamic_cast<NamespaceNode*>(decl.get())) {
                for (auto& nsDecl : ns->declarations) {
                    if (auto* fn = dynamic_cast<FunctionNode*>(nsDecl.get())) {
                        FunctionInfo info;
                        info.name = fn->name;
                        info.namespaceName = ns->name;
                        info.returnType = fn->returnType.name;
                        for (auto& param : fn->parameters) {
                            info.parameters.emplace_back(param->type.name, param->name);
                        }
                        m_functions.emplace_back(std::move(info));
                    }
                }
            }
            if (auto* fn = dynamic_cast<FunctionNode*>(decl.get())) {
                FunctionInfo info;
                info.name = fn->name;
                info.returnType = fn->returnType.name;
                for (auto& param : fn->parameters) {
                    info.parameters.emplace_back(param->type.name, param->name);
                }
                m_functions.emplace_back(std::move(info));
            }
        }
    }
    catch (...) {
        // parsing failed
    }
}

std::string LanguageServer::readMessage() {
    std::string header;
    int contentLength = 0;
    // read headers
    while (true) {
        std::string line;
        std::getline(std::cin, line);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        if (line.starts_with("Content-Length: ")) {
            contentLength = std::stoi(line.substr(16));
        }
    }
    if (contentLength == 0) return "";
    std::string content(contentLength, '\0');
    std::cin.read(content.data(), contentLength);
    return content;
}

void LanguageServer::sendMessage(const nlohmann::json& message) {
    std::string content = message.dump();
    std::cout << "Content-Length: " << content.size() << "\r\n\r\n" << content;
    std::cout.flush();
}

void LanguageServer::sendResponse(const nlohmann::json& id, const nlohmann::json& result) {
    sendMessage({
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", result}
    });
}

void LanguageServer::handleMessage(const nlohmann::json& message) {
    std::string method = message["method"];
    auto id = message.contains("id") ? message["id"] : nlohmann::json(nullptr);
    auto params = message.contains("params") ? message["params"] : nlohmann::json({});
    
    if (method == "initialize") handleInitialize(id, params);
    else if (method == "textDocument/completion") handleCompletion(id, params);
    else if (method == "textDocument/signatureHelp") handleSignatureHelp(id, params);
    else if (method == "textDocument/hover") handleHover(id, params);
    else if (method == "textDocument/didOpen") {
        auto& textDoc = params["textDocument"];
        std::string uri = textDoc["uri"].get<std::string>();
        std::string text = textDoc["text"].get<std::string>();
        parseSource(text);
        // log to stderr for debugging (won't interfere with stdout LSP messages)
        std::cerr << "Opened file: " << uri << " functions found: " << m_functions.size() << "\n";
    }
    else if (method == "textDocument/didChange") {
        auto& changes = params["contentChanges"];
        if (!changes.empty()) {
            std::string text = changes[0]["text"].get<std::string>();
            parseSource(text);
            std::cerr << "File changed, functions found: " << m_functions.size() << "\n";
        }
    }
    else if (method == "initialized") {} // no-op
    else if (method == "shutdown") sendResponse(id, nullptr);
    else if (method == "exit") exit(0);
}

void LanguageServer::handleInitialize(const nlohmann::json& id, const nlohmann::json& params) {
    sendResponse(id, {
        {"capabilities", {
            {"textDocumentSync", 1},
            {"completionProvider", {
                {"triggerCharacters", {":", "."}}
            }},
            {"signatureHelpProvider", {
                {"triggerCharacters", {"(", ","}}
            }},
            {"hoverProvider", true}
        }}
    });
}

void LanguageServer::handleCompletion(const nlohmann::json& id, const nlohmann::json& params) {
    auto& textDoc = params["textDocument"];
    auto& pos = params["position"];
    
    std::string uri = textDoc["uri"];
    std::string path = uri;
    // strip file:/// prefix
    if (path.starts_with("file:///")) path = path.substr(8);
    std::replace(path.begin(), path.end(), '/', '\\');
    
    std::ifstream file(path);
    if (file.is_open()) {
        std::string source((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        parseSource(source);
    }
    
    nlohmann::json items = nlohmann::json::array();
    for (auto& fn : m_functions) {
        nlohmann::json item;
        item["label"] = fn.name;
        item["kind"] = 3; // Function
        // returnType namespace::name(params)
        std::string detail = fn.returnType + " " + 
                            (fn.namespaceName.empty() ? "" : fn.namespaceName + "::") + 
                            fn.name + "(";
        for (size_t i = 0; i < fn.parameters.size(); i++) {
            detail += fn.parameters[i].first + " " + fn.parameters[i].second;
            if (i < fn.parameters.size() - 1) detail += ", ";
        }
        detail += ")";
        item["detail"] = detail;
        items.push_back(item);
    }
    
    sendResponse(id, {{"isIncomplete", false}, {"items", items}});
}

void LanguageServer::handleSignatureHelp(const nlohmann::json& id, const nlohmann::json& params) {
    nlohmann::json signatures = nlohmann::json::array();
    
    // find the function being called from context
    for (auto& fn : m_functions) {
        nlohmann::json sig;
        std::string label = fn.returnType + " " + fn.name + "(";
        nlohmann::json parameters = nlohmann::json::array();
        for (size_t i = 0; i < fn.parameters.size(); i++) {
            std::string paramLabel = fn.parameters[i].first + " " + fn.parameters[i].second;
            label += paramLabel;
            if (i < fn.parameters.size() - 1) label += ", ";
            parameters.push_back({{"label", paramLabel}});
        }
        label += ")";
        sig["label"] = label;
        sig["parameters"] = parameters;
        signatures.push_back(sig);
    }
    
    sendResponse(id, {
        {"signatures", signatures},
        {"activeSignature", 0},
        {"activeParameter", 0}
    });
}

void LanguageServer::handleHover(const nlohmann::json& id, const nlohmann::json& params) {
    // TODO: extract word at position and find matching function
    sendResponse(id, {{"contents", ""}});
}
