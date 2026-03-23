/*
* File: dsl_compiler.cpp
* Project: Cisalpine Engine
* Author: Collin Longoria
* Created on: 3/4/2026
*
* Copyright (c) 2025 Collin Longoria
*
* This software is released under the MIT License.
* https://opensource.org/licenses/MIT
*
* DSL Compiler implementation: Three-stage pipeline (Lex → Parse → Emit GLSL).
*/

#include "compiler.hpp"
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cisalpine {

const std::vector<VariableAllocation> DSLCompiler::emptyVars = {};

// ============================================================
// Lexer
// ============================================================

bool Lexer::isKeyword(const std::string& word, TokenType& out) {
    static const std::map<std::string, TokenType> keywords = {
        {"DEFINE",      TokenType::DEFINE},
        {"SET",         TokenType::SET},
        {"ADD",         TokenType::ADD},
        {"IF",          TokenType::IF},
        {"AND",         TokenType::AND},
        {"INTERACT",    TokenType::INTERACT},
        {"ROLL",        TokenType::ROLL},
        {"TURN",        TokenType::TURN},
        {"CREATE",      TokenType::CREATE},
        {"MAKES",       TokenType::MAKES},
        {"BURN",        TokenType::BURN},
        {"SWARM",       TokenType::SWARM},
        {"ABOVE",       TokenType::ABOVE},
        {"BELOW",       TokenType::BELOW},
        {"LEFTOF",      TokenType::LEFTOF},
        {"RIGHTOF",     TokenType::RIGHTOF},
        {"HEIGHTABOVE", TokenType::HEIGHTABOVE},
        {"HEIGHTBELOW", TokenType::HEIGHTBELOW},
        {"NEARBY",      TokenType::NEARBY},
        {"LESSTHAN",    TokenType::LESSTHAN},
        {"GREATERTHAN", TokenType::GREATERTHAN},
        {"EQUAL",       TokenType::EQUAL},
        {"HOT",         TokenType::HOT},
        {"COLD",        TokenType::COLD},
    };

    auto it = keywords.find(word);
    if (it != keywords.end()) {
        out = it->second;
        return true;
    }
    return false;
}

std::vector<Token> Lexer::tokenize(const std::string& source) {
    std::vector<Token> tokens;
    int line = 1;
    size_t i = 0;

    while (i < source.size()) {
        char c = source[i];

        // Skip whitespace
        if (c == ' ' || c == '\t' || c == '\r') {
            i++;
            continue;
        }

        // Newline
        if (c == '\n') {
            line++;
            i++;
            continue;
        }

        // Comment (// to end of line)
        if (c == '/' && i + 1 < source.size() && source[i + 1] == '/') {
            while (i < source.size() && source[i] != '\n') i++;
            continue;
        }

        // Number
        if (c >= '0' && c <= '9') {
            std::string num;
            while (i < source.size() && source[i] >= '0' && source[i] <= '9') {
                num += source[i++];
            }
            tokens.emplace_back(TokenType::NUMBER, num, line);
            continue;
        }

        // Negative number
        if (c == '-' && i + 1 < source.size() && source[i + 1] >= '0' && source[i + 1] <= '9') {
            std::string num;
            num += source[i++]; // the '-'
            while (i < source.size() && source[i] >= '0' && source[i] <= '9') {
                num += source[i++];
            }
            tokens.emplace_back(TokenType::NUMBER, num, line);
            continue;
        }

        // Identifier or keyword
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_') {
            std::string word;
            while (i < source.size() &&
                   ((source[i] >= 'A' && source[i] <= 'Z') ||
                    (source[i] >= 'a' && source[i] <= 'z') ||
                    (source[i] >= '0' && source[i] <= '9') ||
                    source[i] == '_')) {
                word += source[i++];
            }

            // Convert to uppercase for keyword matching
            std::string upper = word;
            std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

            TokenType kw;
            if (isKeyword(upper, kw)) {
                tokens.emplace_back(kw, upper, line);
            } else {
                // Keep original case for identifiers (element names, var names)
                tokens.emplace_back(TokenType::IDENTIFIER, word, line);
            }
            continue;
        }

        // Unknown character - skip with warning
        std::cerr << "DSL Lexer: Unexpected character '" << c << "' at line " << line << std::endl;
        i++;
    }

    tokens.emplace_back(TokenType::END_OF_FILE, "", line);
    return tokens;
}

static void findMakesNodes(const ASTNodePtr& node, bool& makesHot, bool& makesCold) {
    if (!node) return;
    if (auto makes = std::dynamic_pointer_cast<MakesNode>(node)) {
        if (makes->isHot) makesHot = true;
        else makesCold = true;
    } else if (auto roll = std::dynamic_pointer_cast<RollNode>(node)) {
        findMakesNodes(roll->trueAction, makesHot, makesCold);
        findMakesNodes(roll->falseAction, makesHot, makesCold);
    } else if (auto ifNode = std::dynamic_pointer_cast<IfNode>(node)) {
        for (auto& c : ifNode->body) findMakesNodes(c, makesHot, makesCold);
    } else if (auto interact = std::dynamic_pointer_cast<InteractNode>(node)) {
        for (auto& c : interact->actions) findMakesNodes(c, makesHot, makesCold);
    } else if (auto block = std::dynamic_pointer_cast<BlockNode>(node)) {
        for (auto& c : block->children) findMakesNodes(c, makesHot, makesCold);
    }
}

// Note: These generateGLSL methods use placeholder patterns.
// The DSLCompiler::emitGLSL method wraps them with proper variable
// read/write logic using the bit-pack allocations.

std::string DefineNode::generateGLSL(const std::string& indent) const {
    // DEFINE doesn't generate runtime GLSL - it's handled at initialization
    return "";
}

std::string SetNode::generateGLSL(const std::string& indent) const {
    // Will be replaced with proper bit-pack write by the compiler
    return indent + "/* SET " + varName + " " + std::to_string(value) + " */\n";
}

std::string AddNode::generateGLSL(const std::string& indent) const {
    return indent + "/* ADD " + varName + " " + std::to_string(value) + " */\n";
}

std::string TurnNode::generateGLSL(const std::string& indent) const {
    // Convert element name to uppercase #define name
    std::string upper = targetElement;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    return indent + "curState.r = " + upper + ";\n"
         + indent + "curState.g = 0u;\n"
         + indent + "customData = 0u;\n"
         + indent + "changed = true;\n";
}

std::string CreateNode::generateGLSL(const std::string& indent) const {
    std::string upper = targetElement;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    return indent + "tryCreateAdjacent(pos, " + upper + ");\n";
}

std::string MakesNode::generateGLSL(const std::string& indent) const {
    if (isHot) {
        return indent + "// MAKES HOT - element radiates heat\n"
             + indent + "emitHeat = true;\n";
    } else {
        return indent + "// MAKES COLD - element radiates cold\n"
             + indent + "emitCold = true;\n";
    }
}

std::string RollNode::generateGLSL(const std::string& indent) const {
    std::string glsl;
    float prob = static_cast<float>(probability) / 100.0f;

    glsl += indent + "if (random01(pos) < " + std::to_string(prob) + ") {\n";
    if (trueAction) {
        glsl += trueAction->generateGLSL(indent + "    ");
    }
    glsl += indent + "}";
    if (falseAction) {
        glsl += " else {\n";
        glsl += falseAction->generateGLSL(indent + "    ");
        glsl += indent + "}";
    }
    glsl += "\n";
    return glsl;
}

std::string IfNode::generateGLSL(const std::string& indent) const {
    std::string glsl;

    // Build condition string
    std::string condStr;
    for (size_t i = 0; i < conditions.size(); i++) {
        if (i > 0) condStr += " && ";

        const Condition& cond = conditions[i];

        // Variable read will be injected by the compiler
        // Built-in variables start with dsl_ and are already declared
        std::string varRead;
        if (cond.varName.substr(0, 4) == "dsl_") {
            varRead = cond.varName; // Built-in: use directly
        } else {
            varRead = "VAR_" + cond.varName; // User-defined: placeholder
        }
        std::string op;
        switch (cond.op) {
            case TokenType::LESSTHAN:    op = " < "; break;
            case TokenType::GREATERTHAN: op = " > "; break;
            case TokenType::EQUAL:       op = " == "; break;
            default: op = " == "; break;
        }
        condStr += "(" + varRead + op + std::to_string(cond.value) + "u)";
    }

    glsl += indent + "if (" + condStr + ") {\n";
    for (const auto& node : body) {
        glsl += node->generateGLSL(indent + "    ");
    }
    glsl += indent + "}\n";
    return glsl;
}

std::string InteractNode::generateGLSL(const std::string& indent) const {
    std::string upper = targetElement;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    std::string glsl;
    std::string foundVar = "found_" + upper;

    glsl += indent + "{\n";
    glsl += indent + "    bool " + foundVar + " = false;\n";

    if (dirX != 0 || dirY != 0) {
        // Directional check - only check specific neighbor
        glsl += indent + "    if (getElement(pos + ivec2(" +
                std::to_string(dirX) + ", " + std::to_string(dirY) + ")) == " + upper + ") {\n";
        glsl += indent + "        " + foundVar + " = true;\n";
        glsl += indent + "    }\n";
    } else {
        // Original behavior - check all 8 neighbors
        glsl += indent + "    for (int dy = -1; dy <= 1; dy++) {\n";
        glsl += indent + "        for (int dx = -1; dx <= 1; dx++) {\n";
        glsl += indent + "            if (dx == 0 && dy == 0) continue;\n";
        glsl += indent + "            if (getElement(pos + ivec2(dx, dy)) == " + upper + ") {\n";
        glsl += indent + "                " + foundVar + " = true;\n";
        glsl += indent + "            }\n";
        glsl += indent + "        }\n";
        glsl += indent + "    }\n";
    }

    glsl += indent + "    if (" + foundVar + ") {\n";
    for (const auto& action : actions) {
        glsl += action->generateGLSL(indent + "        ");
    }
    glsl += indent + "    }\n";
    glsl += indent + "}\n";
    return glsl;
}

std::string SwarmNode::generateGLSL(const std::string& indent) const {
    std::string glsl;

    glsl += indent + "// SWARM: boid-like flocking\n";
    glsl += indent + "{\n";
    glsl += indent + "    // Cohesion + alignment: average position of nearby same-element cells\n";
    glsl += indent + "    vec2 cohesion = vec2(0.0);\n";
    glsl += indent + "    vec2 separation = vec2(0.0);\n";
    glsl += indent + "    int neighborCount = 0;\n";
    glsl += indent + "    int radius = " + std::to_string(cohesionRadius) + ";\n";
    glsl += indent + "    int sepDist = " + std::to_string(separationDist) + ";\n";
    glsl += indent + "    for (int sdy = -radius; sdy <= radius; sdy++) {\n";
    glsl += indent + "        for (int sdx = -radius; sdx <= radius; sdx++) {\n";
    glsl += indent + "            if (sdx == 0 && sdy == 0) continue;\n";
    glsl += indent + "            ivec2 np = pos + ivec2(sdx, sdy);\n";
    glsl += indent + "            if (!inBounds(np)) continue;\n";
    glsl += indent + "            if (getElement(np) == elem) {\n";
    glsl += indent + "                cohesion += vec2(float(sdx), float(sdy));\n";
    glsl += indent + "                neighborCount++;\n";
    glsl += indent + "                float d = length(vec2(float(sdx), float(sdy)));\n";
    glsl += indent + "                if (d < float(sepDist) + 0.5 && d > 0.0) {\n";
    glsl += indent + "                    separation -= vec2(float(sdx), float(sdy)) / max(d, 0.1);\n";
    glsl += indent + "                }\n";
    glsl += indent + "            }\n";
    glsl += indent + "        }\n";
    glsl += indent + "    }\n";
    glsl += indent + "    if (neighborCount > 0) {\n";
    glsl += indent + "        cohesion /= float(neighborCount);\n";
    glsl += indent + "        vec2 steer = cohesion * 0.4 + separation * 0.6;\n";
    glsl += indent + "        float steerLen = length(steer);\n";
    glsl += indent + "        if (steerLen > 0.1) {\n";
    glsl += indent + "            steer /= steerLen;\n";
    glsl += indent + "            // Add randomness for natural look\n";
    glsl += indent + "            float rng = random01(pos);\n";
    glsl += indent + "            steer.x += (rng - 0.5) * 0.5;\n";
    glsl += indent + "            steer.y += (random01b(pos) - 0.5) * 0.5;\n";
    glsl += indent + "            int moveX = int(sign(steer.x));\n";
    glsl += indent + "            int moveY = int(sign(steer.y));\n";
    glsl += indent + "            ivec2 target = pos + ivec2(moveX, moveY);\n";
    glsl += indent + "            if (inBounds(target) && getElement(target) == EMPTY) {\n";
    glsl += indent + "                if (random01(pos) < 0.7) {\n";
    glsl += indent + "                    imageStore(stateOut, target, uvec4(elem, curState.g, curState.b, curState.a));\n";
    glsl += indent + "                    curState.r = EMPTY;\n";
    glsl += indent + "                    curState.g = 0u;\n";
    glsl += indent + "                    changed = true;\n";
    glsl += indent + "                    wakeChunkAndNeighbors(target);\n";
    glsl += indent + "                    return;\n";
    glsl += indent + "                }\n";
    glsl += indent + "            }\n";
    glsl += indent + "        }\n";
    glsl += indent + "    } else {\n";
    glsl += indent + "        // No neighbors: wander randomly\n";
    glsl += indent + "        float rng = random01(pos);\n";
    glsl += indent + "        int wx = int(rng * 3.0) - 1;\n";
    glsl += indent + "        int wy = int(random01b(pos) * 3.0) - 1;\n";
    glsl += indent + "        ivec2 wTarget = pos + ivec2(wx, wy);\n";
    glsl += indent + "        if (inBounds(wTarget) && getElement(wTarget) == EMPTY) {\n";
    glsl += indent + "            if (random01(pos) < 0.7) {\n";
    glsl += indent + "                imageStore(stateOut, wTarget, uvec4(elem, curState.g, curState.b, curState.a));\n";
    glsl += indent + "                curState.r = EMPTY;\n";
    glsl += indent + "                curState.g = 0u;\n";
    glsl += indent + "                changed = true;\n";
    glsl += indent + "                wakeChunkAndNeighbors(wTarget);\n";
    glsl += indent + "                return;\n";
    glsl += indent + "            }\n";
    glsl += indent + "        }\n";
    glsl += indent + "    }\n";
    glsl += indent + "}\n";

    return glsl;
}

std::string BlockNode::generateGLSL(const std::string& indent) const {
    std::string glsl;
    for (const auto& child : children) {
        glsl += child->generateGLSL(indent);
    }
    return glsl;
}

// ============================================================
// DSL Compiler: Parsing
// ============================================================

static const Token& peek(const std::vector<Token>& tokens, size_t pos) {
    if (pos < tokens.size()) return tokens[pos];
    static Token eof(TokenType::END_OF_FILE, "", 0);
    return eof;
}

static bool match(const std::vector<Token>& tokens, size_t pos, TokenType type) {
    return pos < tokens.size() && tokens[pos].type == type;
}

static int expectNumber(const std::vector<Token>& tokens, size_t& pos) {
    if (!match(tokens, pos, TokenType::NUMBER)) {
        throw std::runtime_error("DSL Parser: Expected number at token " + std::to_string(pos));
    }
    int val = std::stoi(tokens[pos].value);
    pos++;
    return val;
}

static std::string expectIdentifier(const std::vector<Token>& tokens, size_t& pos) {
    if (pos >= tokens.size()) {
        throw std::runtime_error("DSL Parser: Expected identifier at end of input");
    }
    // Accept IDENTIFIER tokens directly
    if (tokens[pos].type == TokenType::IDENTIFIER) {
        std::string val = tokens[pos].value;
        pos++;
        return val;
    }
    // Also accept keyword tokens that might be element names.
    // Keywords are uppercased by the lexer, so convert back to title case
    // for element name matching (e.g., "HOT" -> "Hot", "COLD" -> "Cold").
    // This allows scripts like "INTERACT Hot" where Hot is both a keyword and element name.
    TokenType t = tokens[pos].type;
    if (t == TokenType::HOT || t == TokenType::COLD ||
        t == TokenType::BURN || t == TokenType::SET ||
        t == TokenType::ADD || t == TokenType::CREATE ||
        t == TokenType::TURN || t == TokenType::ROLL ||
        t == TokenType::MAKES) {
        // Reconstruct as title-case identifier
        std::string val = tokens[pos].value;
        if (!val.empty()) {
            for (size_t i = 1; i < val.size(); i++) val[i] = std::tolower(val[i]);
        }
        pos++;
        return val;
    }
    throw std::runtime_error("DSL Parser: Expected identifier at token " + std::to_string(pos));
}

ASTNodePtr DSLCompiler::parseAction(const std::vector<Token>& tokens, size_t& pos) {
    const Token& tok = peek(tokens, pos);

    switch (tok.type) {
        case TokenType::TURN: {
            pos++;
            auto node = std::make_shared<TurnNode>();
            node->targetElement = expectIdentifier(tokens, pos);
            return node;
        }
        case TokenType::CREATE: {
            pos++;
            auto node = std::make_shared<CreateNode>();
            node->targetElement = expectIdentifier(tokens, pos);
            return node;
        }
        case TokenType::MAKES: {
            pos++;
            auto node = std::make_shared<MakesNode>();
            if (match(tokens, pos, TokenType::HOT)) {
                node->isHot = true;
                pos++;
            } else if (match(tokens, pos, TokenType::COLD)) {
                node->isHot = false;
                pos++;
            } else {
                throw std::runtime_error("DSL Parser: MAKES expects HOT or COLD");
            }
            return node;
        }
        case TokenType::BURN: {
            // BURN is sugar for: ROLL 90 TURN Fire TURN Smoke
            pos++;
            auto node = std::make_shared<RollNode>();
            node->probability = 90;

            auto turnFire = std::make_shared<TurnNode>();
            turnFire->targetElement = "Fire";
            node->trueAction = turnFire;

            auto turnSmoke = std::make_shared<TurnNode>();
            turnSmoke->targetElement = "Smoke";
            node->falseAction = turnSmoke;

            return node;
        }
        case TokenType::SET: {
            pos++;
            auto node = std::make_shared<SetNode>();
            node->varName = expectIdentifier(tokens, pos);
            node->value = expectNumber(tokens, pos);
            return node;
        }
        case TokenType::ADD: {
            pos++;
            auto node = std::make_shared<AddNode>();
            node->varName = expectIdentifier(tokens, pos);
            node->value = expectNumber(tokens, pos);
            return node;
        }
        case TokenType::ROLL: {
            pos++;
            auto node = std::make_shared<RollNode>();
            node->probability = expectNumber(tokens, pos);
            node->trueAction = parseAction(tokens, pos);
            // Check if there's a false action (the next token is still an action, not a new statement)
            const Token& next = peek(tokens, pos);
            if (next.type == TokenType::TURN || next.type == TokenType::CREATE ||
                next.type == TokenType::MAKES || next.type == TokenType::BURN ||
                next.type == TokenType::ROLL || next.type == TokenType::SET ||
                next.type == TokenType::ADD || next.type == TokenType::SWARM) {
                node->falseAction = parseAction(tokens, pos);
            }
            return node;
        }
        case TokenType::SWARM: {
            pos++;
            auto node = std::make_shared<SwarmNode>();
            // Optional: SWARM radius separation
            if (match(tokens, pos, TokenType::NUMBER)) {
                node->cohesionRadius = expectNumber(tokens, pos);
            }
            if (match(tokens, pos, TokenType::NUMBER)) {
                node->separationDist = expectNumber(tokens, pos);
            }
            return node;
        }
        default:
            throw std::runtime_error("DSL Parser: Unexpected token '" + tok.value +
                                     "' when expecting action at position " + std::to_string(pos));
    }
}

ASTNodePtr DSLCompiler::parseStatement(const std::vector<Token>& tokens, size_t& pos) {
    const Token& tok = peek(tokens, pos);

    switch (tok.type) {
        case TokenType::DEFINE: {
            pos++;
            auto node = std::make_shared<DefineNode>();
            node->varName = expectIdentifier(tokens, pos);
            node->initialValue = expectNumber(tokens, pos);
            return node;
        }
        case TokenType::INTERACT: {
            pos++;
            auto node = std::make_shared<InteractNode>();

            // Check for directional modifier before element name
            if (match(tokens, pos, TokenType::ABOVE)) {
                node->dirX = 0; node->dirY = 1; pos++;
            } else if (match(tokens, pos, TokenType::BELOW)) {
                node->dirX = 0; node->dirY = -1; pos++;
            } else if (match(tokens, pos, TokenType::LEFTOF)) {
                node->dirX = -1; node->dirY = 0; pos++;
            } else if (match(tokens, pos, TokenType::RIGHTOF)) {
                node->dirX = 1; node->dirY = 0; pos++;
            }

            node->targetElement = expectIdentifier(tokens, pos);
            // Parse subsequent actions until we hit another top-level statement or EOF
            while (pos < tokens.size()) {
                const Token& next = peek(tokens, pos);
                // Stop at next top-level keyword
                if (next.type == TokenType::INTERACT ||
                    next.type == TokenType::IF ||
                    next.type == TokenType::DEFINE ||
                    next.type == TokenType::SWARM ||
                    next.type == TokenType::END_OF_FILE) {
                    break;
                }
                node->actions.push_back(parseAction(tokens, pos));
            }
            return node;
        }
        case TokenType::IF: {
            pos++;
            auto node = std::make_shared<IfNode>();

            // Parse first condition - can be IDENTIFIER or built-in token
            Condition cond;
            const Token& condTok = peek(tokens, pos);
            if (condTok.type == TokenType::HEIGHTABOVE) {
                cond.varName = "dsl_heightAbove";
                pos++;
            } else if (condTok.type == TokenType::HEIGHTBELOW) {
                cond.varName = "dsl_heightBelow";
                pos++;
            } else if (condTok.type == TokenType::NEARBY) {
                cond.varName = "dsl_nearby";
                pos++;
            } else if (condTok.type == TokenType::HOT) {
                cond.varName = "dsl_hot";
                pos++;
            } else if (condTok.type == TokenType::COLD) {
                cond.varName = "dsl_cold";
                pos++;
            } else {
                cond.varName = expectIdentifier(tokens, pos);
            }

            // Parse operator
            const Token& opTok = peek(tokens, pos);
            if (opTok.type == TokenType::LESSTHAN ||
                opTok.type == TokenType::GREATERTHAN ||
                opTok.type == TokenType::EQUAL) {
                cond.op = opTok.type;
                pos++;
                cond.value = expectNumber(tokens, pos);
            } else {
                if (cond.varName == "dsl_hot" || cond.varName == "dsl_cold" || cond.varName == "dsl_nearby") {
                    cond.op = TokenType::GREATERTHAN;
                    cond.value = 0;
                } else {
                    throw std::runtime_error("DSL Parser: Expected operator (LESSTHAN/GREATERTHAN/EQUAL)");
                }
            }
            node->conditions.push_back(cond);

            // Parse AND chains
            while (match(tokens, pos, TokenType::AND)) {
                pos++; // consume AND
                Condition andCond;
                const Token& andCondTok = peek(tokens, pos);
                if (andCondTok.type == TokenType::HEIGHTABOVE) {
                    andCond.varName = "dsl_heightAbove";
                    pos++;
                } else if (andCondTok.type == TokenType::HEIGHTBELOW) {
                    andCond.varName = "dsl_heightBelow";
                    pos++;
                } else if (andCondTok.type == TokenType::NEARBY) {
                    andCond.varName = "dsl_nearby";
                    pos++;
                } else if (andCondTok.type == TokenType::HOT) {
                    andCond.varName = "dsl_hot";
                    pos++;
                } else if (andCondTok.type == TokenType::COLD) {
                    andCond.varName = "dsl_cold";
                    pos++;
                } else {
                    andCond.varName = expectIdentifier(tokens, pos);
                }
                const Token& andOp = peek(tokens, pos);
                if (andOp.type == TokenType::LESSTHAN ||
                    andOp.type == TokenType::GREATERTHAN ||
                    andOp.type == TokenType::EQUAL) {
                    andCond.op = andOp.type;
                    pos++;
                    andCond.value = expectNumber(tokens, pos);
                } else {
                    if (andCond.varName == "dsl_hot" || andCond.varName == "dsl_cold" || andCond.varName == "dsl_nearby") {
                        andCond.op = TokenType::GREATERTHAN;
                        andCond.value = 0;
                    } else {
                        throw std::runtime_error("DSL Parser: Expected operator after AND");
                    }
                }
                node->conditions.push_back(andCond);
            }

            // Parse body actions
            while (pos < tokens.size()) {
                const Token& next = peek(tokens, pos);
                if (next.type == TokenType::INTERACT ||
                    next.type == TokenType::IF ||
                    next.type == TokenType::DEFINE ||
                    next.type == TokenType::SWARM ||
                    next.type == TokenType::END_OF_FILE) {
                    break;
                }
                node->body.push_back(parseAction(tokens, pos));
            }
            return node;
        }
        default:
            // Try parsing as a standalone action (TURN, CREATE, ROLL, SWARM, etc.)
            return parseAction(tokens, pos);
    }
}

std::vector<ASTNodePtr> DSLCompiler::parseBody(const std::vector<Token>& tokens, size_t& pos) {
    std::vector<ASTNodePtr> nodes;
    while (pos < tokens.size() && tokens[pos].type != TokenType::END_OF_FILE) {
        nodes.push_back(parseStatement(tokens, pos));
    }
    return nodes;
}

// ============================================================
// Bit-Pack Allocation
// ============================================================

int DSLCompiler::bitsNeeded(int maxValue) const {
    if (maxValue <= 0) return 1;
    int bits = 0;
    int v = maxValue;
    while (v > 0) {
        bits++;
        v >>= 1;
    }
    return bits;
}

uint32_t DSLCompiler::generateMask(int bitOffset, int bitWidth) const {
    uint32_t mask = (1u << bitWidth) - 1u;
    return mask << bitOffset;
}

void DSLCompiler::allocateVariables(ElementScript& script) {
    int currentBit = 0;

    for (auto& node : script.rootNodes) {
        auto defNode = std::dynamic_pointer_cast<DefineNode>(node);
        if (!defNode) continue;

        // Determine how many bits this variable needs.
        // We'll allocate enough for reasonable use. If DEFINE FOO 0, we still
        // need some bits. Use a minimum of 4 bits (max value 15) by default,
        // or enough bits to hold the initial value + reasonable headroom.
        int minBits = bitsNeeded(std::max(defNode->initialValue, 15));
        if (minBits < 4) minBits = 4;
        if (minBits > 16) minBits = 16; // Cap at 16 bits per variable

        if (currentBit + minBits > 32) {
            std::cerr << "DSL Compiler: Out of custom data bits for element '"
                      << script.elementName << "' variable '" << defNode->varName << "'" << std::endl;
            continue;
        }

        VariableAllocation alloc;
        alloc.name = defNode->varName;
        alloc.bitOffset = currentBit;
        alloc.bitWidth = minBits;
        alloc.initialValue = defNode->initialValue;

        defNode->bitOffset = currentBit;
        defNode->bitWidth = minBits;

        script.variables.push_back(alloc);
        currentBit += minBits;
    }

    script.totalBitsUsed = currentBit;
}

std::string DSLCompiler::generateReadVar(const VariableAllocation& var) const {
    uint32_t mask = (1u << var.bitWidth) - 1u;
    std::ostringstream ss;
    ss << "((customData >> " << var.bitOffset << "u) & 0x"
       << std::hex << mask << std::dec << "u)";
    return ss.str();
}

std::string DSLCompiler::generateWriteVar(const VariableAllocation& var, const std::string& valueExpr) const {
    uint32_t mask = (1u << var.bitWidth) - 1u;
    uint32_t clearMask = ~(mask << var.bitOffset);
    std::ostringstream ss;
    ss << "customData = (customData & 0x" << std::hex << clearMask << std::dec << "u) | "
       << "((" << valueExpr << " & 0x" << std::hex << mask << std::dec << "u) << " << var.bitOffset << "u)";
    return ss.str();
}

// ============================================================
// Compilation
// ============================================================

bool DSLCompiler::compile(const std::string& elementName, int elementId, const std::string& source) {
    if (source.empty()) return true; // No script is valid

    try {
        Lexer lexer;
        auto tokens = lexer.tokenize(source);

        ElementScript script;
        script.elementName = elementName;
        script.elementId = elementId;

        size_t pos = 0;
        script.rootNodes = parseBody(tokens, pos);

        // Allocate bit-pack storage for DEFINEd variables
        allocateVariables(script);

        scripts.push_back(std::move(script));

        std::cout << "DSL: Compiled script for '" << elementName
                  << "' (ID " << elementId << ", "
                  << scripts.back().rootNodes.size() << " statements, "
                  << scripts.back().totalBitsUsed << "/32 custom bits)" << std::endl;

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "DSL Compile Error for '" << elementName << "': " << e.what() << std::endl;
        return false;
    }
}

// ============================================================
// GLSL Emission
// ============================================================

std::string DSLCompiler::emitGLSL() const {
    if (scripts.empty()) {
        return "// No DSL scripts compiled\n"
               "void executeCustomBehaviors(ivec2 pos, inout uvec4 curState, inout uint customData) {}\n";
    }

    std::ostringstream glsl;

    // Helper: tryCreateAdjacent
    glsl << "// --- BEGIN GENERATED DSL LOGIC ---\n\n";

    glsl << "// Helper: attempt to spawn element in an adjacent EMPTY cell\n";
    glsl << "void tryCreateAdjacent(ivec2 pos, uint element) {\n";
    glsl << "    // Check all 8 neighbors for an empty cell\n";
    glsl << "    for (int dy = -1; dy <= 1; dy++) {\n";
    glsl << "        for (int dx = -1; dx <= 1; dx++) {\n";
    glsl << "            if (dx == 0 && dy == 0) continue;\n";
    glsl << "            ivec2 np = pos + ivec2(dx, dy);\n";
    glsl << "            if (inBounds(np) && getElement(np) == EMPTY) {\n";
    glsl << "                // Use hash to only sometimes succeed (avoids race conditions)\n";
    glsl << "                if (random01(np) < 0.5) {\n";
    glsl << "                    imageStore(stateOut, np, uvec4(element, 0u, 0u, 0u));\n";
    glsl << "                    wakeChunkAndNeighbors(np);\n";
    glsl << "                    return;\n";
    glsl << "                }\n";
    glsl << "            }\n";
    glsl << "        }\n";
    glsl << "    }\n";
    glsl << "}\n\n";

    // Main dispatch function
    glsl << "void executeCustomBehaviors(ivec2 pos, inout uvec4 curState, inout uint customData) {\n";
    glsl << "    uint elem = curState.r;\n";
    glsl << "    bool changed = false;\n";
    glsl << "    bool emitHeat = false;\n";
    glsl << "    bool emitCold = false;\n\n";

    // Built-in readable values
    std::vector<std::string> hotElements;
    std::vector<std::string> coldElements;
    for (const auto& script : scripts) {
        bool isHot = false, isCold = false;
        for (const auto& node : script.rootNodes) {
            findMakesNodes(node, isHot, isCold);
        }
        std::string upper = script.elementName;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        if (isHot) hotElements.push_back(upper);
        if (isCold) coldElements.push_back(upper);
    }

    // Built-in readable values
    glsl << "    // Built-in readable values\n";
    glsl << "    uint dsl_heightAbove = uint(int(worldSize.y) - pos.y);\n";
    glsl << "    uint dsl_heightBelow = uint(pos.y);\n";
    glsl << "    uint dsl_nearby = 0u;\n";
    glsl << "    uint dsl_hot = 0u;\n";
    glsl << "    uint dsl_cold = 0u;\n";
    glsl << "    for (int ny_ = -1; ny_ <= 1; ny_++) {\n";
    glsl << "        for (int nx_ = -1; nx_ <= 1; nx_++) {\n";
    glsl << "            if (nx_ == 0 && ny_ == 0) continue;\n";
    glsl << "            if (inBounds(pos + ivec2(nx_, ny_))) {\n";
    glsl << "                uint ne = getElement(pos + ivec2(nx_, ny_));\n";
    glsl << "                if (ne == elem) dsl_nearby++;\n";
    if (!hotElements.empty()) {
        glsl << "                if (";
        for (size_t i = 0; i < hotElements.size(); i++) {
            glsl << "ne == " << hotElements[i] << (i + 1 == hotElements.size() ? "" : " || ");
        }
        glsl << ") dsl_hot++;\n";
    }
    if (!coldElements.empty()) {
        glsl << "                if (";
        for (size_t i = 0; i < coldElements.size(); i++) {
            glsl << "ne == " << coldElements[i] << (i + 1 == coldElements.size() ? "" : " || ");
        }
        glsl << ") dsl_cold++;\n";
    }
    glsl << "            }\n";
    glsl << "        }\n";
    glsl << "    }\n\n";

    // OPEN THE SWITCH STATEMENT HERE
    glsl << "    switch (elem) {\n";

    for (const auto& script : scripts) {
        // Convert element name to uppercase for the #define
        std::string upper = script.elementName;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

        glsl << "        case " << upper << ": {\n";

        // Generate variable read helpers as local variables
        for (const auto& var : script.variables) {
            std::string readExpr = generateReadVar(var);
            glsl << "            uint " << var.name << " = " << readExpr << ";\n";
        }

        // Generate GLSL for each statement
        for (const auto& node : script.rootNodes) {
            // Skip DEFINE nodes (handled above as initialization)
            if (std::dynamic_pointer_cast<DefineNode>(node)) continue;

            // SwarmNode can be at top-level
            if (auto swarmNode = std::dynamic_pointer_cast<SwarmNode>(node)) {
                glsl << swarmNode->generateGLSL("            ");
                continue;
            }

            // For SET/ADD nodes, we need to post-process the GLSL to replace
            // variable references with proper bit-pack operations
            std::string nodeGlsl = node->generateGLSL("            ");

            // Replace VAR_xxx placeholders with actual bit-pack reads
            for (const auto& var : script.variables) {
                std::string placeholder = "VAR_" + var.name;
                std::string readExpr = var.name; // Use the local variable we declared above
                size_t searchPos = 0;
                while ((searchPos = nodeGlsl.find(placeholder, searchPos)) != std::string::npos) {
                    nodeGlsl.replace(searchPos, placeholder.length(), readExpr);
                    searchPos += readExpr.length();
                }
            }

            // Replace SET/ADD comments with actual bit operations
            for (const auto& var : script.variables) {
                // Replace SET comments
                {
                    std::string setComment = "/* SET " + var.name + " ";
                    size_t searchPos = 0;
                    while ((searchPos = nodeGlsl.find(setComment, searchPos)) != std::string::npos) {
                        size_t endComment = nodeGlsl.find(" */", searchPos);
                        if (endComment != std::string::npos) {
                            // Extract value
                            size_t valStart = searchPos + setComment.length();
                            std::string valStr = nodeGlsl.substr(valStart, endComment - valStart);
                            std::string writeExpr = generateWriteVar(var, valStr + "u") + ";\n";
                            // Also update the local variable
                            writeExpr += "            " + var.name + " = " + valStr + "u;\n";

                            // Find the full line (including indent and newline)
                            size_t lineStart = nodeGlsl.rfind('\n', searchPos);
                            if (lineStart == std::string::npos) lineStart = 0; else lineStart++;
                            size_t lineEnd = nodeGlsl.find('\n', endComment);
                            if (lineEnd == std::string::npos) lineEnd = nodeGlsl.size();
                            else lineEnd++;

                            nodeGlsl.replace(lineStart, lineEnd - lineStart,
                                             "            " + writeExpr);
                            searchPos = lineStart + writeExpr.length() + 12;
                        } else {
                            searchPos++;
                        }
                    }
                }

                // Replace ADD comments
                {
                    std::string addComment = "/* ADD " + var.name + " ";
                    size_t searchPos = 0;
                    while ((searchPos = nodeGlsl.find(addComment, searchPos)) != std::string::npos) {
                        size_t endComment = nodeGlsl.find(" */", searchPos);
                        if (endComment != std::string::npos) {
                            size_t valStart = searchPos + addComment.length();
                            std::string valStr = nodeGlsl.substr(valStart, endComment - valStart);
                            std::string addExpr = "(" + var.name + " + " + valStr + "u)";
                            std::string writeExpr = generateWriteVar(var, addExpr) + ";\n";
                            writeExpr += "            " + var.name + " = " + addExpr + ";\n";

                            size_t lineStart = nodeGlsl.rfind('\n', searchPos);
                            if (lineStart == std::string::npos) lineStart = 0; else lineStart++;
                            size_t lineEnd = nodeGlsl.find('\n', endComment);
                            if (lineEnd == std::string::npos) lineEnd = nodeGlsl.size();
                            else lineEnd++;

                            nodeGlsl.replace(lineStart, lineEnd - lineStart,
                                             "            " + writeExpr);
                            searchPos = lineStart + writeExpr.length() + 12;
                        } else {
                            searchPos++;
                        }
                    }
                }
            }

            glsl << nodeGlsl;
        }

        // Write back variable state if any variables exist
        if (!script.variables.empty()) {
            glsl << "            // Write updated custom data back\n";
        }

        glsl << "            break;\n";
        glsl << "        }\n";
    }

    glsl << "        default: break;\n";
    glsl << "    }\n\n";

    glsl << "    if (changed) {\n";
    glsl << "        wakeChunkAndNeighbors(pos);\n";
    glsl << "    }\n";

    glsl << "}\n";
    glsl << "// --- END GENERATED DSL LOGIC ---\n";

    return glsl.str();
}

const std::vector<VariableAllocation>& DSLCompiler::getVariables(int elementId) const {
    for (const auto& script : scripts) {
        if (script.elementId == elementId) {
            return script.variables;
        }
    }
    return emptyVars;
}

uint32_t DSLCompiler::getInitialCustomData(int elementId) const {
    for (const auto& script : scripts) {
        if (script.elementId == elementId) {
            uint32_t data = 0;
            for (const auto& var : script.variables) {
                uint32_t mask = (1u << var.bitWidth) - 1u;
                data |= (static_cast<uint32_t>(var.initialValue) & mask) << var.bitOffset;
            }
            return data;
        }
    }
    return 0;
}

} // namespace cisalpine