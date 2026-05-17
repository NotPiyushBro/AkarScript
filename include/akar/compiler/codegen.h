#pragma once
#include "ast.h"
#include "akar/common/chunk.h"
#include <unordered_map>
#include <set>
#include <string>

namespace akar {

// Local variable in register
struct Local {
    std::string name;
    int register_id;
    int depth;
    bool is_captured = false;
};

// Upvalue reference
struct UpvalueRef {
    uint8_t index;
    bool is_local;
};

// Compilation scope (function level)
struct CompilerScope {
    ObjFunction* function = nullptr;
    std::vector<Local> locals;
    std::vector<UpvalueRef> upvalues;
    int scope_depth = 0;
    int next_register = 0;
    int max_registers = 0;
    CompilerScope* enclosing = nullptr;

    // Break/continue jump targets
    std::vector<int> break_jumps;
    std::vector<int> continue_targets;
};

class CodeGenerator {
public:
    CodeGenerator();
    ObjFunction* compile(const ASTPtr& ast);
    ObjFunction* compile_function(const std::string& name, const std::vector<Param>& params, const ASTPtr& body);

private:
    // Expression compilation returns the register holding the result
    int compile_expr(const ASTPtr& node);
    void compile_stmt(const ASTPtr& node);

    void compile_number(NumberLiteral* node, int reg);
    void compile_string(StringLiteral* node, int reg);
    void compile_bool(BoolLiteral* node, int reg);
    void compile_nil(NilLiteral* node, int reg);
    void compile_array(ArrayLiteral* node, int reg);
    void compile_map(MapLiteral* node, int reg);
    void compile_identifier(Identifier* node, int reg);
    void compile_binary(BinaryExpr* node, int reg);
    void compile_unary(UnaryExpr* node, int reg);
    void compile_logical(LogicalExpr* node, int reg);
    void compile_call(CallExpr* node, int reg);
    void compile_index(IndexExpr* node, int reg);
    void compile_field_access(FieldAccessExpr* node, int reg);
    void compile_field_set(FieldSetExpr* node, int reg);
    void compile_assignment(AssignmentExpr* node, int reg);
    void compile_array_index_set(ArrayIndexSetExpr* node, int reg);
    void compile_range(RangeExpr* node, int reg);
    void compile_this(ThisExpr* node, int reg);
    void compile_super(SuperAccessExpr* node, int reg);

    void compile_block(BlockStmt* node);
    void compile_if(IfStmt* node);
    void compile_while(WhileStmt* node);
    void compile_for(ForStmt* node);
    void compile_for_in(ForInStmt* node);
    void compile_break(BreakStmt* node);
    void compile_continue(ContinueStmt* node);
    void compile_return(ReturnStmt* node);
    void compile_let(LetStmt* node);
    void compile_fn(FnStmt* node);
    void compile_class(ClassStmt* node);
    void compile_include(IncludeStmt* node);
    void compile_await(AwaitStmt* node);
    void compile_destructuring(DestructuringStmt* node);
    void compile_switch(SwitchStmt* node);
    void compile_try_catch(TryCatchStmt* node);
    void compile_throw(ThrowStmt* node);
    void compile_expr_stmt(ExprStmt* node);

    // Register allocation
    int alloc_register();
    void free_register();
    int current_register() const;

    // Scope management
    void begin_scope();
    void end_scope();
    void declare_local(const std::string& name, int reg);
    int resolve_local(const std::string& name) const;
    int resolve_upvalue(const std::string& name);
    int add_upvalue(uint8_t index, bool is_local);

    // Emission helpers
    void emit(uint8_t op, uint8_t a, uint8_t b, uint8_t c);
    void emit_bx(uint8_t op, uint8_t a, uint16_t bx);
    void emit_wide(uint8_t op, uint16_t a, uint16_t b, uint16_t c);
    size_t emit_jump(uint8_t op, uint8_t a, int16_t offset = 0);
    void patch_jump(size_t jump_byte_pos, int16_t offset);
    size_t current_offset() const;
    size_t add_constant(Value value);
    uint16_t make_constant(Value value);
    uint16_t make_identifier_constant(const std::string& name);

    CompilerScope* current_scope_ = nullptr;
    std::unordered_map<std::string, Value> global_names_;
    int string_counter_ = 0;

    // Include support
    std::string base_path_;
    std::set<std::string> visited_files_;

    // Symbol stripping: track which string values are identifiers vs literals
    std::set<std::string> identifier_values_;
    std::set<std::string> literal_values_;

public:
    void set_base_path(const std::string& path) { base_path_ = path; }
    const std::set<std::string>& identifier_values() const { return identifier_values_; }
    const std::set<std::string>& literal_values() const { return literal_values_; }
};

} // namespace akar
