/*
* File: compiler.hpp
* Project: Cisalpine Engine
* Author: Collin Longoria
* Created on: 3/4/2026
*
* Copyright (c) 2025 Collin Longoria
*
* This software is released under the MIT License.
* https://opensource.org/licenses/MIT
*
*/

#ifndef CISALPINE_DSL_COMPILER_HPP
#define CISALPINE_DSL_COMPILER_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>

namespace cisalpine {

enum class TokenType {
    // Keywords
    DEFINE,
    SET,
    ADD,
    IF,
    AND,
    INTERACT,
    ROLL,
    TURN,
    CREATE,
    MAKES,
    BURN,
    EXPLODE,
    SWARM,       // Boid-like flocking behavior
    ANY,         // Wildcard: matches any non-empty, non-indestructible element

    // Direction keywords
    ABOVE,       // Check/interact with cell above
    BELOW,       // Check/interact with cell below
    LEFTOF,      // Check/interact with cell left
    RIGHTOF,     // Check/interact with cell right

    // Built-in readable values
    HEIGHTABOVE, // Pixel Y-distance to top of world
    HEIGHTBELOW, // Pixel Y-distance to bottom of world
    NEARBY,      // Count of same-element neighbors

    // Operators
    LESSTHAN,
    GREATERTHAN,
    EQUAL,

    // Temperature states
    HOT,
    COLD,

    // Literals
    NUMBER,
    IDENTIFIER,

    // End of stream
    END_OF_FILE
};

struct Token {
    TokenType type;
    std::string value;
    int line = 0;

    Token() = default;
    Token(TokenType t, const std::string& v, int ln = 0)
        : type(t), value(v), line(ln) {}
};

class Lexer {
public:
    std::vector<Token> tokenize(const std::string& source);

private:
    static bool isKeyword(const std::string& word, TokenType& out);
};

// Forward declarations
struct ASTNode;
using ASTNodePtr = std::shared_ptr<ASTNode>;

// Base AST node
struct ASTNode {
    virtual ~ASTNode() = default;
    virtual std::string generateGLSL(const std::string& indent = "    ") const = 0;
};

// DEFINE VAR_NAME INITIAL_VALUE
struct DefineNode : public ASTNode {
    std::string varName;
    int initialValue;
    int bitOffset;  // Assigned by compiler during bit-pack allocation
    int bitWidth;   // Assigned by compiler during bit-pack allocation

    std::string generateGLSL(const std::string& indent = "    ") const override;
};

// SET VAR_NAME VALUE
struct SetNode : public ASTNode {
    std::string varName;
    int value;

    std::string generateGLSL(const std::string& indent = "    ") const override;
};

// ADD VAR_NAME VALUE
struct AddNode : public ASTNode {
    std::string varName;
    int value;

    std::string generateGLSL(const std::string& indent = "    ") const override;
};

// TURN ELEMENT_NAME
struct TurnNode : public ASTNode {
    std::string targetElement;

    std::string generateGLSL(const std::string& indent = "    ") const override;
};

// CREATE ELEMENT_NAME
struct CreateNode : public ASTNode {
    std::string targetElement;

    std::string generateGLSL(const std::string& indent = "    ") const override;
};

// MAKES HOT / MAKES COLD
struct MakesNode : public ASTNode {
    bool isHot; // true = HOT, false = COLD

    std::string generateGLSL(const std::string& indent = "    ") const override;
};

// ROLL CHANCE TRUE_ACTION FALSE_ACTION
struct RollNode : public ASTNode {
    int probability; // 0-100
    ASTNodePtr trueAction;
    ASTNodePtr falseAction; // May be nullptr

    std::string generateGLSL(const std::string& indent = "    ") const override;
};

// IF VAR_NAME OPERATOR VALUE { actions... }
struct Condition {
    std::string varName;
    TokenType op; // LESSTHAN, GREATERTHAN, EQUAL
    int value;
};

struct IfNode : public ASTNode {
    std::vector<Condition> conditions; // IF + AND chains
    std::vector<ASTNodePtr> body;

    std::string generateGLSL(const std::string& indent = "    ") const override;
};

// INTERACT ELEMENT_NAME { actions... }
// Optional direction: ABOVE/BELOW/LEFT/RIGHT restricts neighbor check
struct InteractNode : public ASTNode {
    std::string targetElement;
    std::vector<ASTNodePtr> actions; // Actions executed when neighbor is found
    int dirX = 0;  // 0 = any direction (original behavior)
    int dirY = 0;  // Non-zero = specific direction check
    bool isAny = false; // true = match any non-empty, non-indestructible neighbor

    std::string generateGLSL(const std::string& indent = "    ") const override;
};

// SWARM - boid-like flocking behavior
// Generates GLSL that moves the element toward nearby same-type neighbors
struct SwarmNode : public ASTNode {
    int cohesionRadius = 5;   // How far to look for neighbors
    int separationDist = 1;   // Minimum distance to maintain

    std::string generateGLSL(const std::string& indent = "    ") const override;
};

// Block of actions (for grouping)
struct BlockNode : public ASTNode {
    std::vector<ASTNodePtr> children;

    std::string generateGLSL(const std::string& indent = "    ") const override;
};

// EXPLODE STRENGTH
struct ExplodeNode : public ASTNode {
    int strength;
    std::string generateGLSL(const std::string& indent = "    ") const override;
};

struct VariableAllocation {
    std::string name;
    int bitOffset;
    int bitWidth;
    int initialValue;
};

// Per-element script data
struct ElementScript {
    std::string elementName;
    int elementId;
    std::vector<ASTNodePtr> rootNodes;
    std::vector<VariableAllocation> variables;
    int totalBitsUsed = 0;
};

class DSLCompiler {
public:
    DSLCompiler() = default;

    // Compile a script string for a given element.
    // elementName: The element this script belongs to (e.g. "Lava")
    // elementId: The numeric ID (used for switch/case generation)
    // source: The raw DSL script text
    // Returns true on success.
    bool compile(const std::string& elementName, int elementId, const std::string& source);

    // After all elements are compiled, generate the full GLSL function body.
    // This produces the executeCustomBehaviors() function and helper functions.
    std::string emitGLSL() const;

    // Get variable allocations for a specific element (for brush initialization)
    const std::vector<VariableAllocation>& getVariables(int elementId) const;

    // Get the initial customData value for an element (all DEFINEd vars packed)
    uint32_t getInitialCustomData(int elementId) const;

    // Check if any scripts were compiled
    bool hasScripts() const { return !scripts.empty(); }

private:
    std::vector<ElementScript> scripts;

    // Parser internals
    ASTNodePtr parseAction(const std::vector<Token>& tokens, size_t& pos);
    ASTNodePtr parseStatement(const std::vector<Token>& tokens, size_t& pos);
    std::vector<ASTNodePtr> parseBody(const std::vector<Token>& tokens, size_t& pos);

    // Bit-pack allocation
    void allocateVariables(ElementScript& script);
    int bitsNeeded(int maxValue) const;

    // GLSL generation helpers
    std::string generateReadVar(const VariableAllocation& var) const;
    std::string generateWriteVar(const VariableAllocation& var, const std::string& valueExpr) const;
    uint32_t generateMask(int bitOffset, int bitWidth) const;

    // Static empty vector for getVariables fallback
    static const std::vector<VariableAllocation> emptyVars;
};

} // namespace cisalpine

#endif // CISALPINE_DSL_COMPILER_HPP