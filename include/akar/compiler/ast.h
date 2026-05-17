#pragma once
#include "akar/common/value.h"
#include <string>
#include <vector>
#include <memory>

namespace akar {

// AST Node types
enum class NodeType {
    // Literals
    NumberLit, StringLit, BoolLit, NilLit,
    ArrayLit, MapLit,

    // Expressions
    Identifier, Binary, Unary, Logical,
    Call, Index, FieldAccess, FieldSet,
    Assignment, ArrayIndexSet,
    Range,
    This, SuperAccess,

    // Statements
    ExprStmt, Block, IfStmt, WhileStmt, ForStmt,
    ForInStmt, BreakStmt, ContinueStmt, ReturnStmt,
    LetStmt, FnStmt, ClassStmt, IncludeStmt,
    AwaitStmt, DestructuringStmt,
    SwitchStmt, TryCatchStmt, ThrowStmt,
};

struct ASTNode {
    NodeType type;
    int line = 0;

    ASTNode(NodeType t, int l = 0) : type(t), line(l) {}
    virtual ~ASTNode() = default;
};

using ASTPtr = std::shared_ptr<ASTNode>;

// ---- Expressions ----

struct NumberLiteral : ASTNode {
    double value;
    NumberLiteral(double v, int l) : ASTNode(NodeType::NumberLit, l), value(v) {}
};

struct StringLiteral : ASTNode {
    std::string value;
    StringLiteral(std::string v, int l) : ASTNode(NodeType::StringLit, l), value(std::move(v)) {}
};

struct BoolLiteral : ASTNode {
    bool value;
    BoolLiteral(bool v, int l) : ASTNode(NodeType::BoolLit, l), value(v) {}
};

struct NilLiteral : ASTNode {
    NilLiteral(int l) : ASTNode(NodeType::NilLit, l) {}
};

struct ArrayLiteral : ASTNode {
    std::vector<ASTPtr> elements;
    ArrayLiteral(int l) : ASTNode(NodeType::ArrayLit, l) {}
};

struct MapLiteral : ASTNode {
    std::vector<std::pair<ASTPtr, ASTPtr>> entries; // key-value pairs
    MapLiteral(int l) : ASTNode(NodeType::MapLit, l) {}
};

struct Identifier : ASTNode {
    std::string name;
    Identifier(std::string n, int l) : ASTNode(NodeType::Identifier, l), name(std::move(n)) {}
};

struct BinaryExpr : ASTNode {
    ASTPtr left;
    std::string op;
    ASTPtr right;
    BinaryExpr(ASTPtr l, std::string o, ASTPtr r, int ln)
        : ASTNode(NodeType::Binary, ln), left(std::move(l)), op(std::move(o)), right(std::move(r)) {}
};

struct UnaryExpr : ASTNode {
    std::string op;
    ASTPtr operand;
    UnaryExpr(std::string o, ASTPtr expr, int l)
        : ASTNode(NodeType::Unary, l), op(std::move(o)), operand(std::move(expr)) {}
};

struct LogicalExpr : ASTNode {
    ASTPtr left;
    std::string op; // "and" or "or"
    ASTPtr right;
    LogicalExpr(ASTPtr l, std::string o, ASTPtr r, int ln)
        : ASTNode(NodeType::Logical, ln), left(std::move(l)), op(std::move(o)), right(std::move(r)) {}
};

struct CallExpr : ASTNode {
    ASTPtr callee;
    std::vector<ASTPtr> arguments;
    CallExpr(ASTPtr c, int l) : ASTNode(NodeType::Call, l), callee(std::move(c)) {}
};

struct IndexExpr : ASTNode {
    ASTPtr object;
    ASTPtr index;
    IndexExpr(ASTPtr obj, ASTPtr idx, int l)
        : ASTNode(NodeType::Index, l), object(std::move(obj)), index(std::move(idx)) {}
};

struct FieldAccessExpr : ASTNode {
    ASTPtr object;
    std::string field;
    FieldAccessExpr(ASTPtr obj, std::string f, int l)
        : ASTNode(NodeType::FieldAccess, l), object(std::move(obj)), field(std::move(f)) {}
};

struct FieldSetExpr : ASTNode {
    ASTPtr object;
    std::string field;
    ASTPtr value;
    FieldSetExpr(ASTPtr obj, std::string f, ASTPtr v, int l)
        : ASTNode(NodeType::FieldSet, l), object(std::move(obj)), field(std::move(f)), value(std::move(v)) {}
};

struct AssignmentExpr : ASTNode {
    std::string name;
    ASTPtr value;
    AssignmentExpr(std::string n, ASTPtr v, int l)
        : ASTNode(NodeType::Assignment, l), name(std::move(n)), value(std::move(v)) {}
};

struct ArrayIndexSetExpr : ASTNode {
    ASTPtr object;
    ASTPtr index;
    ASTPtr value;
    ArrayIndexSetExpr(ASTPtr obj, ASTPtr idx, ASTPtr v, int l)
        : ASTNode(NodeType::ArrayIndexSet, l), object(std::move(obj)), index(std::move(idx)), value(std::move(v)) {}
};

struct RangeExpr : ASTNode {
    ASTPtr start;
    ASTPtr end;
    bool inclusive;
    RangeExpr(ASTPtr s, ASTPtr e, bool inc, int l)
        : ASTNode(NodeType::Range, l), start(std::move(s)), end(std::move(e)), inclusive(inc) {}
};

struct ThisExpr : ASTNode {
    ThisExpr(int l) : ASTNode(NodeType::This, l) {}
};

struct SuperAccessExpr : ASTNode {
    std::string method;
    SuperAccessExpr(std::string m, int l) : ASTNode(NodeType::SuperAccess, l), method(std::move(m)) {}
};

// ---- Statements ----

struct ExprStmt : ASTNode {
    ASTPtr expression;
    ExprStmt(ASTPtr expr, int l) : ASTNode(NodeType::ExprStmt, l), expression(std::move(expr)) {}
};

struct BlockStmt : ASTNode {
    std::vector<ASTPtr> statements;
    BlockStmt(int l) : ASTNode(NodeType::Block, l) {}
};

struct IfStmt : ASTNode {
    ASTPtr condition;
    ASTPtr then_branch;
    ASTPtr else_branch; // nullable
    IfStmt(ASTPtr cond, ASTPtr then_b, ASTPtr else_b, int l)
        : ASTNode(NodeType::IfStmt, l), condition(std::move(cond)),
          then_branch(std::move(then_b)), else_branch(std::move(else_b)) {}
};

struct WhileStmt : ASTNode {
    ASTPtr condition;
    ASTPtr body;
    WhileStmt(ASTPtr cond, ASTPtr b, int l)
        : ASTNode(NodeType::WhileStmt, l), condition(std::move(cond)), body(std::move(b)) {}
};

struct ForStmt : ASTNode {
    ASTPtr init;   // nullable
    ASTPtr cond;   // nullable
    ASTPtr update; // nullable
    ASTPtr body;
    ForStmt(ASTPtr i, ASTPtr c, ASTPtr u, ASTPtr b, int l)
        : ASTNode(NodeType::ForStmt, l), init(std::move(i)), cond(std::move(c)),
          update(std::move(u)), body(std::move(b)) {}
};

struct ForInStmt : ASTNode {
    std::string variable;
    ASTPtr iterable;
    ASTPtr body;
    ForInStmt(std::string var, ASTPtr iter, ASTPtr b, int l)
        : ASTNode(NodeType::ForInStmt, l), variable(std::move(var)),
          iterable(std::move(iter)), body(std::move(b)) {}
};

struct BreakStmt : ASTNode {
    BreakStmt(int l) : ASTNode(NodeType::BreakStmt, l) {}
};

struct ContinueStmt : ASTNode {
    ContinueStmt(int l) : ASTNode(NodeType::ContinueStmt, l) {}
};

struct ReturnStmt : ASTNode {
    std::vector<ASTPtr> values; // 0 = return nil, 1 = single, 2+ = multi
    ReturnStmt(ASTPtr v, int l) : ASTNode(NodeType::ReturnStmt, l) {
        if (v) values.push_back(std::move(v));
    }
    ReturnStmt(std::vector<ASTPtr> vals, int l)
        : ASTNode(NodeType::ReturnStmt, l), values(std::move(vals)) {}
};

struct LetStmt : ASTNode {
    std::string name;
    ASTPtr initializer; // nullable
    LetStmt(std::string n, ASTPtr init, int l)
        : ASTNode(NodeType::LetStmt, l), name(std::move(n)), initializer(std::move(init)) {}
};

struct Param {
    std::string name;
    bool is_variadic = false;
};

struct FnStmt : ASTNode {
    std::string name;
    std::vector<Param> params;
    ASTPtr body; // BlockStmt
    FnStmt(std::string n, std::vector<Param> p, ASTPtr b, int l)
        : ASTNode(NodeType::FnStmt, l), name(std::move(n)),
          params(std::move(p)), body(std::move(b)) {}
};

struct ClassStmt : ASTNode {
    std::string name;
    std::vector<std::shared_ptr<FnStmt>> methods;
    ClassStmt(std::string n, int l) : ASTNode(NodeType::ClassStmt, l), name(std::move(n)) {}
};

struct IncludeStmt : ASTNode {
    std::string path;
    IncludeStmt(std::string p, int l) : ASTNode(NodeType::IncludeStmt, l), path(std::move(p)) {}
};

struct AwaitStmt : ASTNode {
    ASTPtr expr;
    AwaitStmt(ASTPtr e, int l) : ASTNode(NodeType::AwaitStmt, l), expr(std::move(e)) {}
};

struct DestructuringStmt : ASTNode {
    std::vector<std::string> names;
    ASTPtr initializer;
    DestructuringStmt(std::vector<std::string> n, ASTPtr init, int l)
        : ASTNode(NodeType::DestructuringStmt, l), names(std::move(n)), initializer(std::move(init)) {}
};

struct CaseClause {
    std::vector<ASTPtr> values; // case values (can match multiple)
    ASTPtr body;                // block or statement
};

struct SwitchStmt : ASTNode {
    ASTPtr expr;
    std::vector<CaseClause> cases;
    ASTPtr default_body; // nullable
    SwitchStmt(ASTPtr e, int l) : ASTNode(NodeType::SwitchStmt, l), expr(std::move(e)) {}
};

struct TryCatchStmt : ASTNode {
    ASTPtr try_body;
    std::string catch_var;
    ASTPtr catch_body;
    TryCatchStmt(ASTPtr tb, std::string cv, ASTPtr cb, int l)
        : ASTNode(NodeType::TryCatchStmt, l), try_body(std::move(tb)),
          catch_var(std::move(cv)), catch_body(std::move(cb)) {}
};

struct ThrowStmt : ASTNode {
    ASTPtr value;
    ThrowStmt(ASTPtr v, int l) : ASTNode(NodeType::ThrowStmt, l), value(std::move(v)) {}
};

} // namespace akar
