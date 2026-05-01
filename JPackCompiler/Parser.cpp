#include "Parser.h"

#include <algorithm>
#include <iostream>
#include <utility>

#include "Codegen.h"

std::unique_ptr<ProgramNode> Parser::parse() {
    auto program = std::make_unique<ProgramNode>();
    while (!isAtEnd()) {
        program->declarations.emplace_back(parseDeclaration());
    }
    return program;
}

void Parser::printNode(const ASTNode* node, int indent) {
    if (auto* fn = dynamic_cast<const ProgramNode*>(node)) {
        for (auto& stmt : fn->declarations) {
            printNode(stmt.get(), indent + 2);
        }
    }
    if (auto* fn = dynamic_cast<const ParameterNode*>(node)) {
        std::cout << std::string(indent, ' ') << "ParameterNode: " << fn->name << "\n";
    }
    if (auto* fn = dynamic_cast<const AnnotationNode*>(node)) {
        std::cout << std::string(indent, ' ') << "AnnotationNode: " << fn->name << "\n";
        if (!fn->arguments.empty()) {
            std::cout << std::string(indent, ' ') << fn->arguments[0] << "\n";
        }
    }
    if (auto* fn = dynamic_cast<const FunctionNode*>(node)) {
        std::cout << std::string(indent, ' ') << "FunctionNode: " << fn->name << "\n";
        for (auto& para : fn->parameters) {
            printNode(para.get(), indent + 2);
        }
        std::cout << std::string(indent, ' ') << fn->returnType.name << "\n";
        for (auto& stmt : fn->body) {
            printNode(stmt.get(), indent + 2);
        }
        for (auto& ann : fn->annotations) {
            printNode(ann.get(), indent + 2);
        }
    }
    if (auto* fn = dynamic_cast<const ClassNode*>(node)) {
        std::cout << std::string(indent, ' ') << "ClassNode: " << fn->name << "\n";
        for (auto& stmt : fn->publicMembers) {
            printNode(stmt.get(), indent + 2);
        }
        for (auto& stmt : fn->privateMembers) {
            printNode(stmt.get(), indent + 2);
        }
    }
    if (auto* fn = dynamic_cast<const StructNode*>(node)) {
        std::cout << std::string(indent, ' ') << "StructNode: " << fn->name << "\n";
        for (auto& stmt : fn->members) {
            printNode(stmt.get(), indent + 2);
        }
    }
    if (auto* fn = dynamic_cast<const EnumNode*>(node)) {
        std::cout << std::string(indent, ' ') << "EnumNode: " << fn->name << "\n";
        for (auto& member : fn->members) {
            std::cout << std::string(indent, ' ') << "StructMember: " << member.name << "\n";
            if (member.value.has_value()) {
                std::cout << std::string(indent, ' ') << member.value.value() << "\n";
            }
        }
    }
    if (auto* fn = dynamic_cast<const VariableNode*>(node)) {
        std::cout << std::string(indent, ' ') << "VariableNode: " << fn->name << "\n";
        printNode(fn->value.get(), indent + 2);
    }
    if (auto* fn = dynamic_cast<const IfNode*>(node)) {
        std::cout << std::string(indent, ' ') << "IfNode: " << "\n";
        printNode(fn->condition.get(), indent + 2);
        std::cout << std::string(indent, ' ') << "body: " << "\n";
        for (auto& stmt : fn->body) {
            printNode(stmt.get(), indent + 2);
        }
        if (!fn->elseBody.empty()) {
            std::cout << std::string(indent, ' ') << "else body: " << "\n";
            for (auto& stmt : fn->elseBody) {
                printNode(stmt.get(), indent + 2);
            }
        }
    }
    if (auto* fn = dynamic_cast<const WhileNode*>(node)) {
        std::cout << std::string(indent, ' ') << "WhileNode: " << "\n";
        printNode(fn->condition.get(), indent + 2);
        std::cout << std::string(indent, ' ') << "body: " << "\n";
        for (auto& stmt : fn->body) {
            printNode(stmt.get(), indent + 2);
        }
    }
    if (auto* fn = dynamic_cast<const ForNode*>(node)) {
        std::cout << std::string(indent, ' ') << "ForNode: " << "\n";
        printNode(fn->initializer.get(), indent + 2);
        printNode(fn->condition.get(), indent + 2);
        printNode(fn->increment.get(), indent + 2);
        std::cout << std::string(indent, ' ') << "body: " << "\n";
        for (auto& stmt : fn->body) {
            printNode(stmt.get(), indent + 2);
        }
    }
    if (auto* fn = dynamic_cast<const BinaryExprNode*>(node)) {
        std::cout << std::string(indent, ' ') << "BinaryExpr: " << tokenTypeToString(fn->op) << "\n";
        printNode(fn->left.get(), indent + 2);
        printNode(fn->right.get(), indent + 2);
    }
    if (auto* fn = dynamic_cast<const LiteralNode*>(node)) {
        std::cout << std::string(indent, ' ') << "Literal: " << fn->value << "\n";
    }
    if (auto* fn = dynamic_cast<const IdentifierNode*>(node)) {
        std::cout << std::string(indent, ' ') << "Identifier: " << fn->name << "\n";
    }
    if (auto* fn = dynamic_cast<const ReturnNode*>(node)) {
        std::cout << std::string(indent, ' ') << "ReturnNode\n";
        if (fn->returnVal) printNode(fn->returnVal.get(), indent + 2);
    }
}

Token Parser::current() const {
    return m_tokens[m_pos];
}

Token Parser::peek(uint8_t offset) const {
    if (m_pos+offset >= m_tokens.size()) return m_tokens[m_tokens.size()-1];
    return m_tokens[m_pos+offset];
}

Token Parser::advance(uint8_t offset) {
    Token token = current();
    if (m_pos + offset <= m_tokens.size()-1) {
        m_pos += offset;
    }
    return token;
}

bool Parser::isAtEnd() const {
    return current().type == TokenType::END_OF_FILE;
}

std::unique_ptr<ASTNode> Parser::parseDeclaration() {
    std::vector<std::unique_ptr<AnnotationNode>> annotations;
    while (current().type == TokenType::AT) {
        annotations.emplace_back(parseAnnotation());
    }

    if (current().type == TokenType::IDENTIFIER && peek().type == TokenType::IDENTIFIER && peek(2).type == TokenType::LPAREN) {
        return parseFunctionDecl(std::move(annotations));
    }
    
    switch (current().type) {
        case TokenType::NAMESPACE: return parseNamespaceDecl();
        case TokenType::CLASS:  {
            auto classNode = parseClassDecl();
            classNode->annotations = std::move(annotations);
            return classNode;
        }
        case TokenType::STRUCT: return parseStructDecl();
        case TokenType::ENUM:   return parseEnumDecl();
        case TokenType::VOID:
        case TokenType::INT:
        case TokenType::FLOAT:
        case TokenType::DOUBLE:
        case TokenType::BOOL:
        case TokenType::STRING: {
            if (peek(2).type == TokenType::LPAREN) {
                return parseFunctionDecl(std::move(annotations));
            }
            return parseVariableDecl();
        }
        default:
            std::cerr << "Parser ERROR: unexpected token " << tokenTypeToString(current().type) 
                      << " at line " << current().line << "\n";
            advance(); // skip the bad token
            return nullptr;
    }
}

std::unique_ptr<NamespaceNode> Parser::parseNamespaceDecl() {
    auto namespaceNode = std::make_unique<NamespaceNode>();
    namespaceNode->sourceLine = current().line;
    namespaceNode->sourceColumn = current().column;
    advance();
    namespaceNode->name = expect(TokenType::IDENTIFIER).value;
    expect(TokenType::LBRACE);
    while (current().type != TokenType::RBRACE) {
        namespaceNode->declarations.emplace_back(parseDeclaration());
    }
    expect(TokenType::RBRACE);
    return namespaceNode;
}

std::unique_ptr<FunctionNode> Parser::parseFunctionDecl(std::vector<std::unique_ptr<AnnotationNode>> annotations) {
    FunctionNode function;
    for (auto& annotation : annotations) {
        if (annotation->name == "intrinsic") function.isIntrinsic = true;
        if (annotation->name == "returns_command") function.isReturnsCommand = true;
        if (annotation->name == "store_result_intrinsic") function.isStoreResultIntrinsic = true;
        if (annotation->name == "ref_intrinsic") function.isRefIntrinsic = true;
        if (annotation->name == "revoke") function.isRevoke = true;
    }
    function.annotations = std::move(annotations);
    TypeInfo returnType;
    if (current().type == TokenType::IDENTIFIER) {
        returnType.name = advance().value;
    } else {
        returnType.name = tokenTypeToString(advance().type);
    }
    function.returnType = returnType;
    function.name = current().value;
    advance();  // consume the name
    expect(TokenType::LPAREN);
    while (current().type != TokenType::RPAREN) {
        auto parameter = std::make_unique<ParameterNode>();
        parameter->sourceLine = current().line;
        parameter->sourceColumn = current().column;
        // should be something in the tokentype type category
        std::string name = tokenTypeToString(current().type);
        std::ranges::transform(name, name.begin(), [](unsigned char c) { return std::tolower(c); });
        advance();
        
        bool isReference = false;
        if (current().type == TokenType::AMPERSAND) {
            isReference = true;
            advance();
        }
        parameter->type = TypeInfo{.name = name, .isReference = isReference};
        
        // should be TokenType::IDENTIFIER
        parameter->name = current().value;
        function.parameters.emplace_back(std::move(parameter));
        advance();
        
        match(TokenType::COMMA);
    }
    expect(TokenType::RPAREN);
    if (current().type == TokenType::SEMICOLON) {
        // function declaration
    } else if (current().type == TokenType::LBRACE) {
        function.body = parseBlock();
    }
    
    auto node = std::make_unique<FunctionNode>(std::move(function));
    node->sourceLine = current().line;
    node->sourceColumn = current().column;
    return node;
}

std::unique_ptr<ClassNode> Parser::parseClassDecl() {
    auto classNode = std::make_unique<ClassNode>();
    classNode->sourceLine = current().line;
    classNode->sourceColumn = current().column;
    advance();
    classNode->name = expect(TokenType::IDENTIFIER).value;
    expect(TokenType::LBRACE);
    bool isPublic = false; // false = private, true = public
    while (current().type != TokenType::RBRACE) {
        if (current().type == TokenType::PUBLIC) {
            isPublic = true;
            advance();
            expect(TokenType::COLON);
            continue;
        }
        if (current().type == TokenType::PRIVATE) {
            isPublic = false;
            advance();
            expect(TokenType::COLON);
            continue;
        }
        if (isPublic) {
            classNode->publicMembers.emplace_back(parseDeclaration());
        } else {
            classNode->privateMembers.emplace_back(parseDeclaration());
        }
    }
    expect(TokenType::RBRACE);
    match(TokenType::SEMICOLON);
    return classNode;
}

std::unique_ptr<ASTNode> Parser::parseStatement() {
    switch (current().type) {
        case TokenType::IF: return parseIfStatement();
        case TokenType::WHILE: return parseWhileStatement();
        case TokenType::FOR: return parseForStatement();
        case TokenType::RETURN: {
            int line = current().line;
            int col = current().column;
            advance();
            auto val = parseExpression();
            expect(TokenType::SEMICOLON);
            auto node = std::make_unique<ReturnNode>();
            node->sourceLine = line;
            node->sourceColumn = col;
            node->returnVal = std::move(val);
            return node;
        }
        case TokenType::BREAK: {
            int line = current().line;
            int col = current().column;
            advance();
            expect(TokenType::SEMICOLON);
            auto node = std::make_unique<BreakNode>();
            node->sourceLine = line;
            node->sourceColumn = col;
            return node;
        }
        case TokenType::CONTINUE: {
            int line = current().line;
            int col = current().column;
            advance();
            expect(TokenType::SEMICOLON);
            auto node = std::make_unique<ContinueNode>();
            node->sourceLine = line;
            node->sourceColumn = col;
            return node;
        }
        case TokenType::VOID:
        case TokenType::INT:
        case TokenType::FLOAT:
        case TokenType::DOUBLE:
        case TokenType::BOOL:
        case TokenType::STRING: return parseVariableDecl();
        case TokenType::AT: {
            advance(); // consume @
            std::string directive = expect(TokenType::IDENTIFIER).value;
            if (directive == "cmd") {
                auto raw = std::make_unique<RawCommandNode>();
                raw->sourceLine = current().line;
                raw->sourceColumn = current().column;
                raw->command = expect(TokenType::STRING_LITERAL).value;
                expect(TokenType::SEMICOLON);
                return raw;
            }
            break;
        }
        default: {
            if (current().type == TokenType::IDENTIFIER && peek().type == TokenType::IDENTIFIER) {
                return parseVariableDecl(); // IDENTIFIER IDENTIFIER (e.g. "Point p")
            }
            auto expression = parseExpression();
            // check for member access assignment
            if (auto* access = dynamic_cast<MemberAccessNode*>(expression.get())) {
                if (current().type == TokenType::EQUAL) {
                    advance();
                    auto assign = std::make_unique<MemberAssignNode>();
                    assign->object = std::move(access->object);
                    assign->memberName = access->memberName;
                    assign->value = parseExpression();
                    expect(TokenType::SEMICOLON);
                    return assign;
                }
            }
            if (auto* access = dynamic_cast<ArrayAccessNode*>(expression.get())) {
                if (current().type == TokenType::EQUAL) {
                    advance();
                    auto assign = std::make_unique<ArrayAssignNode>();
                    assign->name = access->name;
                    assign->index = std::move(access->index);
                    assign->value = parseExpression();
                    expect(TokenType::SEMICOLON);
                    return assign;
                }
            }
            expect(TokenType::SEMICOLON);
            return expression;
        }
    }
    auto expression = parseExpression();
    expect(TokenType::SEMICOLON);
    return expression;
}

std::unique_ptr<ASTNode> Parser::parseIfStatement() {
    auto ifNode = std::make_unique<IfNode>();
    ifNode->sourceLine = current().line;
    ifNode->sourceColumn = current().column;
    advance();
    expect(TokenType::LPAREN);
    ifNode->condition = parseExpression();
    expect(TokenType::RPAREN);
    ifNode->body = parseBlock();
    if (current().type == TokenType::ELSE) {
        advance();
        if (current().type == TokenType::IF) {
            ifNode->elseBody.emplace_back(parseIfStatement());
        } else {
            ifNode->elseBody = parseBlock();
        }
    }
    return ifNode;
}

std::unique_ptr<ASTNode> Parser::parseWhileStatement() {
    auto whileNode = std::make_unique<WhileNode>();
    whileNode->sourceLine = current().line;
    whileNode->sourceColumn = current().column;
    advance();
    expect(TokenType::LPAREN);
    whileNode->condition = parseExpression();
    expect(TokenType::RPAREN);
    whileNode->body = parseBlock();
    return whileNode;
}

std::unique_ptr<ASTNode> Parser::parseForStatement() {
    auto forNode = std::make_unique<ForNode>();
    forNode->sourceLine = current().line;
    forNode->sourceColumn = current().column;
    advance();
    expect(TokenType::LPAREN);
    forNode->initializer = parseVariableDecl();
    forNode->condition = parseExpression();
    expect(TokenType::SEMICOLON);
    forNode->increment = parseExpression();
    expect(TokenType::RPAREN);
    forNode->body = parseBlock();
    return forNode;
}

std::unique_ptr<ASTNode> Parser::parseExpression(int minBP) {
    auto left = parsePrimary(); // parse the leftmost value
    
    if (current().type == TokenType::MATCHES) {
        advance(); // consume matches
        auto matchesNode = std::make_unique<MatchesExprNode>();
        matchesNode->target = std::move(left);
        // parse range: could be 1..10, ..10, 1.., or just 1
        if (current().type == TokenType::INT_LITERAL) {
            matchesNode->min = std::stoi(advance().value);
            matchesNode->hasMin = true;
        }
        if (current().type == TokenType::RANGE) {
            advance();
            matchesNode->hasRange = true;
            if (current().type == TokenType::INT_LITERAL) {
                matchesNode->max = std::stoi(advance().value);
                matchesNode->hasMax = true;
            }
        }
        return matchesNode;
    }
    
    // handle assignment
    if (current().type == TokenType::EQUAL) {
        advance();
        auto right = parseExpression(0); // right associative, so 0
        auto assign = std::make_unique<BinaryExprNode>();
        assign->sourceLine = current().line;
        assign->sourceColumn = current().column;
        assign->left = std::move(left);
        assign->op = TokenType::EQUAL;
        assign->right = std::move(right);
        return assign;
    }
    
    while (true) {
        int bp = getBindingPower(current().type);
        if (bp <= minBP) break;
        TokenType op = advance().type;
        auto right = parseExpression(bp);
        auto binaryExpression = std::make_unique<BinaryExprNode>();
        binaryExpression->sourceLine = current().line;
        binaryExpression->sourceColumn = current().column;
        binaryExpression->left = std::move(left);
        binaryExpression->op = op;
        binaryExpression->right = std::move(right);
        left = std::move(binaryExpression);
    }
    return left;
}

std::unique_ptr<StructNode> Parser::parseStructDecl() {
    auto structNode = std::make_unique<StructNode>();
    structNode->sourceLine = current().line;
    structNode->sourceColumn = current().column;
    advance();
    structNode->name = expect(TokenType::IDENTIFIER).value;
    expect(TokenType::LBRACE);
    while (current().type != TokenType::RBRACE) {
        structNode->members.emplace_back(parseVariableDecl());
    }
    expect(TokenType::RBRACE);
    match(TokenType::SEMICOLON);
    return structNode;
}

std::unique_ptr<EnumNode> Parser::parseEnumDecl() {
    auto enumNode = std::make_unique<EnumNode>();
    enumNode->sourceLine = current().line;
    enumNode->sourceColumn = current().column;
    advance();
    enumNode->name = expect(TokenType::IDENTIFIER).value;
    expect(TokenType::LBRACE);
    while (current().type != TokenType::RBRACE) {
        EnumMember member;
        member.name = expect(TokenType::IDENTIFIER).value;
        if (current().type == TokenType::EQUAL && peek().type == TokenType::INT_LITERAL) {
            advance();
            member.value = std::optional(std::stoi(advance().value)); // advance also consumes
        }
        match(TokenType::COMMA);
        enumNode->members.emplace_back(member);
    }
    expect(TokenType::RBRACE);
    match(TokenType::SEMICOLON);
    return enumNode;
}

std::unique_ptr<VariableNode> Parser::parseVariableDecl() {
    auto variable = std::make_unique<VariableNode>();
    variable->sourceLine = current().line;
    variable->sourceColumn = current().column;
    TypeInfo typeInfo;
    if (current().type == TokenType::IDENTIFIER) {
        typeInfo.name = advance().value; // capture class/struct names
    } else {
        typeInfo.name = tokenTypeToString(advance().type);
    }
    if (current().type == TokenType::AMPERSAND) {
        typeInfo.isReference = true;
        advance();
    }
    
    variable->type = typeInfo;
    variable->name = expect(TokenType::IDENTIFIER).value;
    
    // check for array type
    if (current().type == TokenType::LBRACK) {
        advance(); // consume [
        if (current().type == TokenType::INT_LITERAL) {
            variable->arraySize = std::stoi(current().value);
            advance();
        } else {
            std::cout << "Parser WARNING: tried to initialize array with dynamic size. This is not yet supported by the compiler!\n";
        }
        expect(TokenType::RBRACK);
        variable->type.isArray = true;
    }
    
    if (current().type == TokenType::EQUAL) {
        advance();
        if (variable->type.isArray && current().type == TokenType::LBRACE) {
            advance(); // consume {
            while (current().type != TokenType::RBRACE) {
                if (current().type == TokenType::COMMA) {
                    advance();
                    continue;
                }
                variable->arrayInitializer.emplace_back(parseExpression());
            }
            advance(); // consume }
        } else {
            variable->value = parseExpression();
        }
    }
    expect(TokenType::SEMICOLON);
    return variable;
}

std::unique_ptr<AnnotationNode> Parser::parseAnnotation() {
    auto annotation = std::make_unique<AnnotationNode>();
    annotation->sourceLine = current().line;
    annotation->sourceColumn = current().column;
    expect(TokenType::AT);
    annotation->name = expect(TokenType::IDENTIFIER).value;
    if (current().type == TokenType::LPAREN) {
        advance();
        while (current().type != TokenType::RPAREN) {
            if (current().type == TokenType::LBRACK) {
                if (peek().type == TokenType::LPAREN) { // templatepool
                    advance(); // consume [
                    while (current().type != TokenType::RBRACK) {
                        if (current().type == TokenType::COMMA) { advance(); continue; }
                        expect(TokenType::LPAREN);
                        std::string location = expect(TokenType::STRING_LITERAL).value;
                        expect(TokenType::COMMA);
                        int weight = std::stoi(expect(TokenType::INT_LITERAL).value);
                        expect(TokenType::RPAREN);
                        annotation->poolEntries.emplace_back(TemplatePoolEntry{location, weight});
                    }
                    advance(); // consume ]
                    continue;
                }
                // JSON array
                annotation->arguments.emplace_back(Codegen::serializeJson(parseJson().get()));
                continue;
            }
            if (current().type == TokenType::COMMA) {
                advance();
                continue;
            }
            if (current().type == TokenType::LBRACE) {
                annotation->arguments.emplace_back(Codegen::serializeJson(parseJson().get()));
                continue;
            }
            annotation->arguments.emplace_back(advance().value);
        }
        expect(TokenType::RPAREN);
    }
    return annotation;
}

std::vector<std::unique_ptr<ASTNode>> Parser::parseBlock() {
    std::vector<std::unique_ptr<ASTNode>> blockNodes;
    expect(TokenType::LBRACE);
    while (current().type != TokenType::RBRACE && current().type != TokenType::END_OF_FILE) {
        blockNodes.emplace_back(parseStatement());
    }
    expect(TokenType::RBRACE);
    return blockNodes;
}

std::unique_ptr<ASTNode> Parser::parsePrimary() {
    std::unique_ptr<ASTNode> result;
    switch (current().type) {
        case TokenType::INT_LITERAL:
        case TokenType::FLOAT_LITERAL:
        case TokenType::DOUBLE_LITERAL:
        case TokenType::STRING_LITERAL:
        case TokenType::BOOL_LITERAL: {
            auto literal = std::make_unique<LiteralNode>();
            literal->sourceLine = current().line;
            literal->sourceColumn = current().column;
            literal->type = current().type;
            literal->value = current().value;
            advance();
            result = std::move(literal);
            break;
        }
        case TokenType::IDENTIFIER: {
            // identifier
            auto identifier = std::make_unique<IdentifierNode>();
            identifier->sourceLine = current().line;
            identifier->sourceColumn = current().column;
            identifier->name = current().value;
            advance();
            
            if (current().type == TokenType::DOUBLE_COLON) {
                advance(); // consume ::
                auto call = std::make_unique<CallNode>();
                call->sourceLine = current().line;
                call->sourceColumn = current().column;
                call->namespaceName = identifier->name;
                call->name = expect(TokenType::IDENTIFIER).value;
                advance(); // consume '('
                while (current().type != TokenType::RPAREN) {
                    call->arguments.emplace_back(parseExpression());
                    match(TokenType::COMMA);
                }
                advance(); // consume ')'
                result = std::move(call);
                break;
            }
            
            // check for postfix ++ or --
            if (current().type == TokenType::INCREMENT || current().type == TokenType::DECREMENT) {
                auto unary = std::make_unique<UnaryExprNode>();
                unary->sourceLine = current().line;
                unary->sourceColumn = current().column;
                unary->op = current().type;
                unary->operand = std::move(identifier);
                advance();
                result = std::move(unary);
                break;
            }
            
            // function
            if (current().type == TokenType::LPAREN) {
                auto functionCall = std::make_unique<CallNode>();
                functionCall->sourceLine = current().line;
                functionCall->sourceColumn = current().column;
                functionCall->name = identifier->name; // use already-captured name
                advance(); // consume '('
                while (current().type != TokenType::RPAREN) {
                    functionCall->arguments.emplace_back(parseExpression());
                    match(TokenType::COMMA);
                }
                advance(); // consume ')'
                result = std::move(functionCall);
                break;
            }
            
            // array access
            if (current().type == TokenType::LBRACK) {
                auto arrayAccess = std::make_unique<ArrayAccessNode>();
                arrayAccess->name = identifier->name;
                advance(); // consume [
                arrayAccess->index = parseExpression();
                expect(TokenType::RBRACK);
                result = std::move(arrayAccess);
                break;
            }
            
            result = std::move(identifier);
            break;
        }
        case TokenType::THIS: {
            auto thisNode = std::make_unique<ThisNode>();
            thisNode->sourceLine = current().line;
            thisNode->sourceColumn = current().column;
            advance();
            result = std::move(thisNode);
            break;
        }
        case TokenType::LPAREN: {
            advance();
            auto expression = parseExpression();
            expect(TokenType::RPAREN);
            result = std::move(expression);
            break;
        }
        case TokenType::LBRACE:
        case TokenType::LBRACK: {
            result = parseJson();
            break;
        }
        case TokenType::NOT:
        case TokenType::MINUS: {
            auto unary = std::make_unique<UnaryExprNode>();
            unary->sourceLine = current().line;
            unary->sourceColumn = current().column;
            unary->op = current().type;
            advance();
            unary->operand = parsePrimary();
            result = std::move(unary);
            break;
        }
        default: result = nullptr;
    }
    
    // method chaining or member access
    while (current().type == TokenType::DOT) {
        advance(); // consume .
        std::string memberName = expect(TokenType::IDENTIFIER).value;
        if (current().type == TokenType::LPAREN) {
            // method call
            auto method = std::make_unique<MethodCallNode>();
            method->sourceLine = current().line;
            method->sourceColumn = current().column;
            method->object = std::move(result);
            method->methodName = memberName;
            advance(); // consume (
            while (current().type != TokenType::RPAREN) {
                method->arguments.emplace_back(parseExpression());
                match(TokenType::COMMA);
            }
            advance(); // consume )
            result = std::move(method);
        } else {
            // member variable access
            auto access = std::make_unique<MemberAccessNode>();
            access->sourceLine = current().line;
            access->sourceColumn = current().column;
            access->object = std::move(result);
            access->memberName = memberName;
            result = std::move(access);
        }
    }
    
    while (current().type == TokenType::DOT) {
        advance(); // consume .
        std::string memberName = expect(TokenType::IDENTIFIER).value;
        if (current().type == TokenType::LPAREN) {
            // method call
            auto method = std::make_unique<MethodCallNode>();
            method->object = std::move(result);
            method->methodName = memberName;
            expect(TokenType::LPAREN);
            while (current().type != TokenType::RPAREN) {
                method->arguments.emplace_back(parseExpression());
                match(TokenType::COMMA);
            }
            advance();
            result = std::move(method);
        } else {
            // member access
            auto access = std::make_unique<MemberAccessNode>();
            access->object = std::move(result);
            access->memberName = memberName;
            result = std::move(access);
        }
    }
    
    return result;
}

static bool toBool(std::string const& s) {
    return s != "false";
}

std::unique_ptr<JsonNode> Parser::parseJson() {
    bool arrayMode = false;
    if (match(TokenType::LBRACK)) {
        arrayMode = true;
    } else {
        expect(TokenType::LBRACE);
    }
    auto rootNode = std::make_unique<JsonNode>();
    rootNode->valueType = arrayMode ? JsonValueType::Array : JsonValueType::Object;
    while (current().type != (arrayMode ? TokenType::RBRACK : TokenType::RBRACE)) {
        auto node = std::make_unique<JsonNode>();
        if (!arrayMode) {
            node->identifier = expect(TokenType::STRING_LITERAL).value;
            expect(TokenType::COLON);
        }
        switch (current().type) {
            case TokenType::STRING_LITERAL: {
                node->valueType = JsonValueType::String;
                node->stringValue = advance().value;
                break;
            }
            case TokenType::INT_LITERAL:
            case TokenType::FLOAT_LITERAL:
            case TokenType::DOUBLE_LITERAL: {
                node->valueType = JsonValueType::Number;
                node->numberValue = std::stod(advance().value);
                break;
            }
            case TokenType::BOOL_LITERAL: {
                node->valueType = JsonValueType::Bool;
                node->boolValue = toBool(advance().value);
                break;
            }
            case TokenType::LBRACE: {
                node->valueType = JsonValueType::Object;
                auto objectNode = parseJson();
                node->objectEntries = std::move(objectNode->objectEntries);
                break;
            }
            case TokenType::LBRACK: {
                node->valueType = JsonValueType::Array;
                auto arrayNode = parseJson();
                node->arrayElements = std::move(arrayNode->arrayElements);
                break;
            }
            case TokenType::IDENTIFIER: {
                if (current().value == "null") {
                    node->valueType = JsonValueType::Null;
                    advance();
                    break;
                }
            }
            default: {
                std::cout << "Unknown json entry: " << tokenTypeToString(current().type) << " value: " << current().value << "\n";
            }
        }
        if (arrayMode) {
            rootNode->arrayElements.emplace_back(std::move(node));
        } else {
            rootNode->objectEntries.emplace_back(node->identifier, std::move(node));
        }
        match(TokenType::COMMA);
    }
    advance(); // consume } or ]
    return rootNode;
}

int Parser::getBindingPower(TokenType type) {
    switch (type) {
        case TokenType::OR: return 1;
        case TokenType::AND: return 2;
        case TokenType::EQUAL_EQUAL:
        case TokenType::NOT_EQUAL: return 3;
        case TokenType::LESSER:
        case TokenType::GREATER:
        case TokenType::LESSER_EQUAL:
        case TokenType::GREATER_EQUAL: return 4;
        case TokenType::PLUS:
        case TokenType::MINUS: return 5;
        case TokenType::TIMES:
        case TokenType::DIVIDE:
        case TokenType::MOD: return 6;
        case TokenType::NOT: return 7;
        case TokenType::DOT:
        case TokenType::LPAREN: return 8;
        case TokenType::INCREMENT:
        case TokenType::DECREMENT:
        case TokenType::RPAREN:
        default: return 0;
    }
}

Token Parser::expect(TokenType type) {
    if (current().type == type) {
        return advance();
    }
    std::cerr << "Parser ERROR: expected '" << tokenTypeToString(type) 
              << "' but found '" << tokenTypeToString(current().type) << "'";
    if (current().line > 0) {
        std::cerr << " at line " << current().line << ", column " << current().column;
    }
    std::cerr << "\n";
    m_errorCount++;
    if (m_errorCount >= MAX_ERRORS) {
        std::cerr << "Parser ERROR: too many errors, stopping\n";
        throw std::runtime_error("too many parse errors");
    }
    return Token{};
}

bool Parser::match(TokenType type) {
    if (current().type == type) {
        advance();
        return true;
    }
    return false;
}

void Parser::synchronize() {
    while (!isAtEnd()) {
        if (current().type == TokenType::SEMICOLON) {
            advance();
            return;
        }
        switch (current().type) {
        case TokenType::VOID:
        case TokenType::INT:
        case TokenType::FLOAT:
        case TokenType::DOUBLE:
        case TokenType::BOOL:
        case TokenType::STRING:
        case TokenType::IF:
        case TokenType::WHILE:
        case TokenType::FOR:
        case TokenType::RETURN:
        case TokenType::NAMESPACE:
        case TokenType::CLASS:
            return;
        default:
            advance();
        }
    }
}
