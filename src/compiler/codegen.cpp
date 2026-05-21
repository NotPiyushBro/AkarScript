#include "akar/compiler/codegen.h"
#include "akar/compiler/lexer.h"
#include "akar/compiler/parser.h"
#include <stdexcept>
#include <cstring>
#include <fstream>
#include <sstream>

namespace akar {

CodeGenerator::CodeGenerator() {}

ObjFunction* CodeGenerator::compile(const ASTPtr& ast) {
    visited_files_.clear();
    auto* func = allocate_function();
    func->name = "<script>";
    func->register_count = 0;

    CompilerScope scope;
    scope.function = func;
    scope.next_register = 0;
    scope.max_registers = 0;
    scope.scope_depth = 0;
    current_scope_ = &scope;

    if (ast->type == NodeType::Block) {
        auto* block = static_cast<BlockStmt*>(ast.get());
        for (auto& stmt : block->statements) {
            compile_stmt(stmt);
        }
    } else {
        compile_stmt(ast);
    }

    // Ensure we return nil at the end
    int ret_reg = alloc_register();
    emit(op_byte(Opcode::LOAD_NIL), ret_reg, 0, 0);
    emit(op_byte(Opcode::RETURN), ret_reg, 0, 0);
    free_register();

    func->register_count = scope.max_registers;
    return func;
}

ObjFunction* CodeGenerator::compile_function(const std::string& name, const std::vector<Param>& params, const ASTPtr& body) {
    auto* func = allocate_function();
    func->name = name;

    // Find variadic param (must be last)
    int variadic_idx = -1;
    for (int i = 0; i < static_cast<int>(params.size()); i++) {
        if (params[i].is_variadic) { variadic_idx = i; break; }
    }
    // Arity is the number of fixed params (variadic doesn't count)
    func->arity = (variadic_idx >= 0) ? variadic_idx : static_cast<int>(params.size());
    func->has_varargs = (variadic_idx >= 0);

    CompilerScope scope;
    scope.function = func;
    scope.next_register = 0;
    scope.max_registers = 0;
    scope.scope_depth = 0;
    scope.enclosing = current_scope_;
    current_scope_ = &scope;

    // Parameters occupy registers 0..arity-1
    for (int i = 0; i < func->arity; i++) {
        int reg = alloc_register();
        declare_local(params[i].name, reg);
    }

    // For variadic param: register the name at the correct register
    // The VM will place the varargs array there at runtime
    if (variadic_idx >= 0) {
        int arr_reg = alloc_register();
        declare_local(params[variadic_idx].name, arr_reg);
    }

    bool is_init = (name == "init");

    // Helper lambda to emit CLOSE_UPVALUE for all captured locals
    auto close_captured = [&]() {
        for (auto& local : scope.locals) {
            if (local.is_captured) {
                emit(op_byte(Opcode::CLOSE_UPVALUE), local.register_id, 0, 0);
            }
        }
    };

    if (body->type == NodeType::Block) {
        auto* block = static_cast<BlockStmt*>(body.get());
        size_t count = block->statements.size();
        for (size_t i = 0; i < count; i++) {
            // Implicit return: if last statement is an expression statement, return it
            // But NOT for init methods (they must return `this`)
            if (!is_init && i == count - 1 && block->statements[i]->type == NodeType::ExprStmt) {
                auto* expr_stmt = static_cast<ExprStmt*>(block->statements[i].get());
                int reg = compile_expr(expr_stmt->expression);
                close_captured();
                emit(op_byte(Opcode::RETURN), reg, 0, 0);
                break;
            }
            compile_stmt(block->statements[i]);
        }
    } else {
        compile_stmt(body);
    }

    // Ensure return (if not already returned by implicit return above)
    auto& bc = scope.function->bytecode;
    bool already_returns = bc.size() >= 4 &&
        (static_cast<Opcode>(bc[bc.size() - 4]) == Opcode::RETURN ||
         (bc.size() >= 8 && static_cast<Opcode>(bc[bc.size() - 8]) == Opcode::WIDE &&
          static_cast<Opcode>(bc[bc.size() - 7]) == Opcode::RETURN));
    if (!already_returns) {
        close_captured();
        int ret_reg = alloc_register();
        if (is_init) {
            emit(op_byte(Opcode::MOVE), ret_reg, 0, 0);
        } else {
            emit(op_byte(Opcode::LOAD_NIL), ret_reg, 0, 0);
        }
        emit(op_byte(Opcode::RETURN), ret_reg, 0, 0);
        free_register();
    }

    func->register_count = scope.max_registers;
    // Peephole optimize the bytecode
    peephole_optimize(func->bytecode);
    // Store upvalue descriptors
    for (auto& uv : scope.upvalues) {
        func->upvalue_descs.push_back({uv.index, uv.is_local});
    }
    current_scope_ = scope.enclosing;
    return func;
}

int CodeGenerator::compile_expr(const ASTPtr& node) {
    int reg = alloc_register();
    switch (node->type) {
        case NodeType::NumberLit:
            compile_number(static_cast<NumberLiteral*>(node.get()), reg);
            break;
        case NodeType::StringLit:
            compile_string(static_cast<StringLiteral*>(node.get()), reg);
            break;
        case NodeType::BoolLit:
            compile_bool(static_cast<BoolLiteral*>(node.get()), reg);
            break;
        case NodeType::NilLit:
            compile_nil(static_cast<NilLiteral*>(node.get()), reg);
            break;
        case NodeType::ArrayLit:
            compile_array(static_cast<ArrayLiteral*>(node.get()), reg);
            break;
        case NodeType::MapLit:
            compile_map(static_cast<MapLiteral*>(node.get()), reg);
            break;
        case NodeType::Identifier:
            compile_identifier(static_cast<Identifier*>(node.get()), reg);
            break;
        case NodeType::Binary:
            compile_binary(static_cast<BinaryExpr*>(node.get()), reg);
            break;
        case NodeType::Unary:
            compile_unary(static_cast<UnaryExpr*>(node.get()), reg);
            break;
        case NodeType::Logical:
            compile_logical(static_cast<LogicalExpr*>(node.get()), reg);
            break;
        case NodeType::Call:
            compile_call(static_cast<CallExpr*>(node.get()), reg);
            break;
        case NodeType::Index:
            compile_index(static_cast<IndexExpr*>(node.get()), reg);
            break;
        case NodeType::FieldAccess:
            compile_field_access(static_cast<FieldAccessExpr*>(node.get()), reg);
            break;
        case NodeType::SuperAccess:
            compile_super(static_cast<SuperAccessExpr*>(node.get()), reg);
            break;
        case NodeType::Range:
            compile_range(static_cast<RangeExpr*>(node.get()), reg);
            break;
        case NodeType::This:
            compile_this(static_cast<ThisExpr*>(node.get()), reg);
            break;
        case NodeType::Assignment:
            compile_assignment(static_cast<AssignmentExpr*>(node.get()), reg);
            break;
        case NodeType::FieldSet:
            compile_field_set(static_cast<FieldSetExpr*>(node.get()), reg);
            break;
        case NodeType::ArrayIndexSet:
            compile_array_index_set(static_cast<ArrayIndexSetExpr*>(node.get()), reg);
            break;
        case NodeType::FnStmt: {
            // Lambda expression
            auto* fn = static_cast<FnStmt*>(node.get());
            auto* func = compile_function(fn->name, fn->params, fn->body);
            uint16_t func_const = make_constant(Value(static_cast<Obj*>(func)));
            emit_bx(op_byte(Opcode::CLOSURE), reg, func_const);
            break;
        }
        default:
            throw std::runtime_error("Unknown expression type");
    }
    return reg;
}

void CodeGenerator::compile_stmt(const ASTPtr& node) {
    switch (node->type) {
        case NodeType::ExprStmt: compile_expr_stmt(static_cast<ExprStmt*>(node.get())); break;
        case NodeType::Block: compile_block(static_cast<BlockStmt*>(node.get())); break;
        case NodeType::IfStmt: compile_if(static_cast<IfStmt*>(node.get())); break;
        case NodeType::WhileStmt: compile_while(static_cast<WhileStmt*>(node.get())); break;
        case NodeType::ForStmt: compile_for(static_cast<ForStmt*>(node.get())); break;
        case NodeType::ForInStmt: compile_for_in(static_cast<ForInStmt*>(node.get())); break;
        case NodeType::BreakStmt: compile_break(static_cast<BreakStmt*>(node.get())); break;
        case NodeType::ContinueStmt: compile_continue(static_cast<ContinueStmt*>(node.get())); break;
        case NodeType::ReturnStmt: compile_return(static_cast<ReturnStmt*>(node.get())); break;
        case NodeType::LetStmt: compile_let(static_cast<LetStmt*>(node.get())); break;
        case NodeType::FnStmt: compile_fn(static_cast<FnStmt*>(node.get())); break;
        case NodeType::ClassStmt: compile_class(static_cast<ClassStmt*>(node.get())); break;
        case NodeType::IncludeStmt: compile_include(static_cast<IncludeStmt*>(node.get())); break;
        case NodeType::AwaitStmt: compile_await(static_cast<AwaitStmt*>(node.get())); break;
        case NodeType::DestructuringStmt: compile_destructuring(static_cast<DestructuringStmt*>(node.get())); break;
        case NodeType::SwitchStmt: compile_switch(static_cast<SwitchStmt*>(node.get())); break;
        case NodeType::TryCatchStmt: compile_try_catch(static_cast<TryCatchStmt*>(node.get())); break;
        case NodeType::ThrowStmt: compile_throw(static_cast<ThrowStmt*>(node.get())); break;
        case NodeType::SignalDeclStmt: compile_signal_decl(static_cast<SignalDeclStmt*>(node.get())); break;
        case NodeType::EffectStmt: compile_effect(static_cast<EffectStmt*>(node.get())); break;
        case NodeType::EnumStmt: compile_enum(static_cast<EnumStmt*>(node.get())); break;
        default:
            // Treat as expression
            { compile_expr(node); free_register(); }
            break;
    }
}

void CodeGenerator::compile_number(NumberLiteral* node, int reg) {
    // Use LOAD_IMM for small non-negative integers (0-255)
    double v = node->value;
    if (v >= 0 && v <= 255 && v == std::floor(v)) {
        emit(op_byte(Opcode::LOAD_IMM), reg, static_cast<int>(v), 0);
    } else {
        uint16_t cx = make_constant(Value(v));
        emit_bx(op_byte(Opcode::LOAD_CONST), reg, cx);
    }
}

void CodeGenerator::compile_string(StringLiteral* node, int reg) {
    auto* str = get_string_table().intern(node->value);
    uint16_t cx = make_constant(Value(static_cast<Obj*>(str)));
    literal_values_.insert(node->value);
    emit_bx(op_byte(Opcode::LOAD_CONST), reg, cx);
}

void CodeGenerator::compile_bool(BoolLiteral* node, int reg) {
    emit(op_byte(node->value ? Opcode::LOAD_TRUE : Opcode::LOAD_FALSE), reg, 0, 0);
}

void CodeGenerator::compile_nil(NilLiteral*, int reg) {
    emit(op_byte(Opcode::LOAD_NIL), reg, 0, 0);
}

void CodeGenerator::compile_array(ArrayLiteral* node, int reg) {
    // Load elements into consecutive registers after reg
    std::vector<int> elem_regs;
    for (auto& elem : node->elements) {
        elem_regs.push_back(compile_expr(elem));
    }
    emit(op_byte(Opcode::NEW_ARRAY), reg, static_cast<uint8_t>(elem_regs.size()), 0);
    for (size_t i = 0; i < elem_regs.size(); i++) {
        int idx_reg = alloc_register();
        uint16_t idx_const = make_constant(Value(static_cast<double>(i)));
        emit_bx(op_byte(Opcode::LOAD_CONST), idx_reg, idx_const);
        emit(op_byte(Opcode::SET_INDEX), reg, idx_reg, elem_regs[i]);
        free_register(); // free idx_reg
    }
    // Free element registers (they're no longer needed after NEW_ARRAY + SET_INDEX)
    for (int i = static_cast<int>(elem_regs.size()) - 1; i >= 0; i--) {
        free_register();
    }
}

void CodeGenerator::compile_map(MapLiteral* node, int reg) {
    emit(op_byte(Opcode::NEW_MAP), reg, 0, 0);
    // Collect key/value regs, then free them after all SET_INDEX
    std::vector<std::pair<int, int>> kv_regs;
    for (auto& [key_node, val_node] : node->entries) {
        int key = compile_expr(key_node);
        int val = compile_expr(val_node);
        kv_regs.push_back({key, val});
    }
    for (auto& [key, val] : kv_regs) {
        emit(op_byte(Opcode::SET_INDEX), reg, key, val);
    }
    // Free all value and key registers (right-to-left to maintain stack order)
    for (int i = static_cast<int>(kv_regs.size()) - 1; i >= 0; i--) {
        free_register(); // free val
        free_register(); // free key
    }
}

void CodeGenerator::compile_identifier(Identifier* node, int reg) {
    // Try local first
    int local = resolve_local(node->name);
    if (local >= 0) {
        // If it's a signal, emit SIGNAL_GET to read its value and track dependencies
        if (signal_set_.count(node->name)) {
            emit(op_byte(Opcode::SIGNAL_GET), reg, local, 0);
        } else {
            emit(op_byte(Opcode::MOVE), reg, local, 0);
        }
        return;
    }
    // Try upvalue
    int upvalue = resolve_upvalue(node->name);
    if (upvalue >= 0) {
        if (signal_set_.count(node->name)) {
            int sig_reg = alloc_register();
            emit(op_byte(Opcode::GET_UPVALUE), sig_reg, upvalue, 0);
            emit(op_byte(Opcode::SIGNAL_GET), reg, sig_reg, 0);
            free_register(); // sig_reg
        } else {
            emit(op_byte(Opcode::GET_UPVALUE), reg, upvalue, 0);
        }
        return;
    }
    // Global: check if it's a signal
    uint16_t name_const = make_identifier_constant(node->name);
    if (signal_set_.count(node->name)) {
        int sig_reg = alloc_register();
        emit_bx(op_byte(Opcode::GET_GLOBAL), sig_reg, name_const);
        emit(op_byte(Opcode::SIGNAL_GET), reg, sig_reg, 0);
        free_register(); // sig_reg
    } else {
        emit_bx(op_byte(Opcode::GET_GLOBAL), reg, name_const);
    }
}

void CodeGenerator::compile_binary(BinaryExpr* node, int reg) {
    // Peephole: x % y == 0 → MOD_EQ_ZERO (fuses 2 opcodes into 1, saves 1M dispatches in sum_primes)
    if (node->op == "==" && node->left->type == NodeType::Binary && node->right->type == NodeType::NumberLit) {
        auto* left_bin = static_cast<BinaryExpr*>(node->left.get());
        auto* right_num = static_cast<NumberLiteral*>(node->right.get());
        if (left_bin->op == "%" && right_num->value == 0.0) {
            // Optimize: use local registers directly for MOD_EQ_ZERO operands
            int mod_left;
            bool ml_local = false;
            if (left_bin->left->type == NodeType::Identifier) {
                auto* id = static_cast<Identifier*>(left_bin->left.get());
                int local = resolve_local(id->name);
                if (local >= 0 && !signal_set_.count(id->name)) { mod_left = local; ml_local = true; }
                else mod_left = compile_expr(left_bin->left);
            } else {
                mod_left = compile_expr(left_bin->left);
            }
            int mod_right;
            bool mr_local = false;
            if (left_bin->right->type == NodeType::Identifier) {
                auto* id = static_cast<Identifier*>(left_bin->right.get());
                int local = resolve_local(id->name);
                if (local >= 0 && !signal_set_.count(id->name)) { mod_right = local; mr_local = true; }
                else mod_right = compile_expr(left_bin->right);
            } else {
                mod_right = compile_expr(left_bin->right);
            }
            emit(op_byte(Opcode::MOD_EQ_ZERO), reg, mod_left, mod_right);
            if (!mr_local) free_register();
            if (!ml_local) free_register();
            return;
        }
    }

    // Optimization: use local registers directly instead of allocating temps + MOVEs
    // But NOT for signals — signals need SIGNAL_GET to read their value
    int left;
    bool left_is_local = false;
    if (node->left->type == NodeType::Identifier) {
        auto* id = static_cast<Identifier*>(node->left.get());
        int local = resolve_local(id->name);
        if (local >= 0 && !signal_set_.count(id->name)) {
            left = local;
            left_is_local = true;
        } else {
            left = compile_expr(node->left);
        }
    } else {
        left = compile_expr(node->left);
    }

    int right;
    bool right_is_local = false;
    if (node->right->type == NodeType::Identifier) {
        auto* id = static_cast<Identifier*>(node->right.get());
        int local = resolve_local(id->name);
        if (local >= 0 && !signal_set_.count(id->name)) {
            right = local;
            right_is_local = true;
        } else {
            right = compile_expr(node->right);
        }
    } else {
        right = compile_expr(node->right);
    }

    Opcode op;
    if (node->op == "+") op = Opcode::ADD;
    else if (node->op == "-") op = Opcode::SUB;
    else if (node->op == "*") op = Opcode::MUL;
    else if (node->op == "/") op = Opcode::DIV;
    else if (node->op == "%") op = Opcode::MOD;
    else if (node->op == "==") op = Opcode::EQ;
    else if (node->op == "!=") op = Opcode::NEQ;
    else if (node->op == "<") op = Opcode::LT;
    else if (node->op == "<=") op = Opcode::LTE;
    else if (node->op == ">") op = Opcode::GT;
    else if (node->op == ">=") op = Opcode::GTE;
    else throw std::runtime_error("Unknown binary operator: " + node->op);

    emit(op_byte(op), reg, left, right);
    if (!right_is_local) free_register();
    if (!left_is_local) free_register();
}

void CodeGenerator::compile_unary(UnaryExpr* node, int reg) {
    int operand = compile_expr(node->operand);
    if (node->op == "-") {
        emit(op_byte(Opcode::NEG), reg, operand, 0);
    } else if (node->op == "!" || node->op == "not") {
        emit(op_byte(Opcode::NOT), reg, operand, 0);
    }
    free_register(); // operand
}

void CodeGenerator::compile_logical(LogicalExpr* node, int reg) {
    if (node->op == "and") {
        int left = compile_expr(node->left);
        emit(op_byte(Opcode::MOVE), reg, left, 0);
        free_register(); // left
        size_t jump = emit_jump(op_byte(Opcode::JMP_IF_FALSE), reg, 0);
        int right = compile_expr(node->right);
        emit(op_byte(Opcode::MOVE), reg, right, 0);
        free_register(); // right
        patch_jump(jump, static_cast<int16_t>(current_offset() - jump - INST_SIZE));
    } else { // "or"
        int left = compile_expr(node->left);
        emit(op_byte(Opcode::MOVE), reg, left, 0);
        free_register(); // left
        size_t jump = emit_jump(op_byte(Opcode::JMP_IF_TRUE), reg, 0);
        int right = compile_expr(node->right);
        emit(op_byte(Opcode::MOVE), reg, right, 0);
        free_register(); // right
        patch_jump(jump, static_cast<int16_t>(current_offset() - jump - INST_SIZE));
    }
}

void CodeGenerator::compile_call(CallExpr* node, int reg) {
    // Check if this is a method call (callee is FieldAccessExpr)
    bool is_method_call = (node->callee->type == NodeType::FieldAccess);
    if (is_method_call) {
        auto* field = static_cast<FieldAccessExpr*>(node->callee.get());
        int object_reg = compile_expr(field->object);
        uint16_t method_const = make_identifier_constant(field->field);
        int callee = alloc_register();
        // Allocate register for "this" arg so arg compilation doesn't reuse it
        int this_slot = alloc_register();

        // Move object to this_slot
        if (object_reg != this_slot) {
            emit(op_byte(Opcode::MOVE), this_slot, object_reg, 0);
        }

        // Compile user args (they'll be allocated at this_slot+1, this_slot+2, ...)
        std::vector<int> arg_regs;
        arg_regs.push_back(this_slot); // "this" is first arg
        for (auto& arg : node->arguments) {
            arg_regs.push_back(compile_expr(arg));
        }

        // Emit GET_FIELD: reads from this_slot (which has the object),
        // writes method closure to callee.
        emit(op_byte(Opcode::GET_FIELD), callee, this_slot, method_const);

        // Move remaining args into position (skip i=0 which is already at this_slot)
        for (int i = 1; i < static_cast<int>(arg_regs.size()); i++) {
            int target = callee + 1 + i;
            if (arg_regs[i] != target) {
                emit(op_byte(Opcode::MOVE), target, arg_regs[i], 0);
            }
        }
        emit(op_byte(Opcode::CALL), callee, static_cast<int>(arg_regs.size()), 0);
        if (callee != reg) {
            emit(op_byte(Opcode::MOVE), reg, callee, 0);
        }
        // Free all temporary registers used for callee and args.
        // After the CALL, only `reg` (the result) needs to survive.
        current_scope_->next_register = reg + 1;
    } else {
        int callee = compile_expr(node->callee);
        std::vector<int> arg_regs;
        for (auto& arg : node->arguments) {
            arg_regs.push_back(compile_expr(arg));
        }
        int call_callee = callee;
        if (callee > 255) {
            call_callee = alloc_register();
            emit(op_byte(Opcode::MOVE), call_callee, callee, 0);
        }
        // Move args left-to-right to avoid clobbering source registers
        for (int i = 0; i < static_cast<int>(arg_regs.size()); i++) {
            int target = call_callee + 1 + i;
            if (arg_regs[i] != target) {
                emit(op_byte(Opcode::MOVE), target, arg_regs[i], 0);
            }
        }
        emit(op_byte(Opcode::CALL), call_callee, static_cast<int>(arg_regs.size()), 0);
        if (call_callee != reg) {
            emit(op_byte(Opcode::MOVE), reg, call_callee, 0);
        }
        // Free all temporary registers used for callee and args.
        // After the CALL, only `reg` (the result) needs to survive.
        current_scope_->next_register = reg + 1;
    }
}

void CodeGenerator::compile_index(IndexExpr* node, int reg) {
    int obj = compile_expr(node->object);
    int idx = compile_expr(node->index);
    emit(op_byte(Opcode::GET_INDEX), reg, obj, idx);
    free_register(); // idx
    free_register(); // obj
}

void CodeGenerator::compile_field_access(FieldAccessExpr* node, int reg) {
    int obj = compile_expr(node->object);
    uint16_t field_const = make_identifier_constant(node->field);
    emit(op_byte(Opcode::GET_FIELD), reg, obj, field_const);
    free_register(); // obj
}

void CodeGenerator::compile_field_set(FieldSetExpr* node, int reg) {
    int obj = compile_expr(node->object);
    int val = compile_expr(node->value);
    uint16_t field_const = make_identifier_constant(node->field);
    emit(op_byte(Opcode::SET_FIELD), obj, field_const, val);
    emit(op_byte(Opcode::MOVE), reg, val, 0);
    free_register(); // val
    free_register(); // obj
}

void CodeGenerator::compile_assignment(AssignmentExpr* node, int reg) {
    // Signal assignment: emit SIGNAL_SET instead of MOVE
    if (signal_set_.count(node->name)) {
        int val = compile_expr(node->value);
        int local = resolve_local(node->name);
        if (local >= 0) {
            emit(op_byte(Opcode::SIGNAL_SET), local, val, 0);
        } else {
            int upvalue = resolve_upvalue(node->name);
            if (upvalue >= 0) {
                int sig_reg = alloc_register();
                emit(op_byte(Opcode::GET_UPVALUE), sig_reg, upvalue, 0);
                emit(op_byte(Opcode::SIGNAL_SET), sig_reg, val, 0);
                free_register(); // sig_reg
            } else {
                // Signal stored as global: load signal object, then SIGNAL_SET
                uint16_t name_const = make_identifier_constant(node->name);
                int sig_reg = alloc_register();
                emit_bx(op_byte(Opcode::GET_GLOBAL), sig_reg, name_const);
                emit(op_byte(Opcode::SIGNAL_SET), sig_reg, val, 0);
                free_register(); // sig_reg
            }
        }
        emit(op_byte(Opcode::MOVE), reg, val, 0);
        free_register(); // val
        return;
    }

    // Peephole: local = local op expr → emit op directly to local register
    // Eliminates 2 MOVEs: one for the temp result, one for the assignment
    if (node->value->type == NodeType::Binary) {
        auto* bin = static_cast<BinaryExpr*>(node->value.get());
        int local = resolve_local(node->name);
        if (local >= 0 && bin->left->type == NodeType::Identifier) {
            auto* left_id = static_cast<Identifier*>(bin->left.get());
            if (resolve_local(left_id->name) == local) {
                // Pattern: local = local op expr
                int right;
                bool right_is_local = false;
                if (bin->right->type == NodeType::Identifier) {
                    auto* right_id = static_cast<Identifier*>(bin->right.get());
                    int rlocal = resolve_local(right_id->name);
                    if (rlocal >= 0) { right = rlocal; right_is_local = true; }
                    else right = compile_expr(bin->right);
                } else {
                    right = compile_expr(bin->right);
                }
                Opcode op;
                if (bin->op == "+") op = Opcode::ADD;
                else if (bin->op == "-") op = Opcode::SUB;
                else if (bin->op == "*") op = Opcode::MUL;
                else if (bin->op == "/") op = Opcode::DIV;
                else if (bin->op == "%") op = Opcode::MOD;
                else goto fallback;
                // Emit: OP local, local, right (in-place update)
                emit(op_byte(op), local, local, right);
                if (!right_is_local) free_register();
                // Assignment result is the local value
                if (reg != local) emit(op_byte(Opcode::MOVE), reg, local, 0);
                return;
            }
        }
    }
fallback:
    int val = compile_expr(node->value);
    int local = resolve_local(node->name);
    if (local >= 0) {
        emit(op_byte(Opcode::MOVE), local, val, 0);
        emit(op_byte(Opcode::MOVE), reg, val, 0);
        free_register(); // val
        return;
    }
    int upvalue = resolve_upvalue(node->name);
    if (upvalue >= 0) {
        emit(op_byte(Opcode::SET_UPVALUE), val, upvalue, 0);
        emit(op_byte(Opcode::MOVE), reg, val, 0);
        free_register(); // val
        return;
    }
    uint16_t name_const = make_identifier_constant(node->name);
    emit_bx(op_byte(Opcode::SET_GLOBAL), val, name_const);
    emit(op_byte(Opcode::MOVE), reg, val, 0);
    free_register(); // val
}

void CodeGenerator::compile_array_index_set(ArrayIndexSetExpr* node, int reg) {
    int obj = compile_expr(node->object);
    int idx = compile_expr(node->index);
    int val = compile_expr(node->value);
    emit(op_byte(Opcode::SET_INDEX), obj, idx, val);
    emit(op_byte(Opcode::MOVE), reg, val, 0);
    free_register(); // val
    free_register(); // idx
    free_register(); // obj
}

void CodeGenerator::compile_range(RangeExpr* node, int reg) {
    int start = compile_expr(node->start);
    int end = compile_expr(node->end);
    emit(op_byte(Opcode::NEW_RANGE), reg, start, end);
    free_register(); // end
    free_register(); // start
}

void CodeGenerator::compile_this(ThisExpr*, int reg) {
    int local = resolve_local("this");
    if (local >= 0) {
        emit(op_byte(Opcode::MOVE), reg, local, 0);
        return;
    }
    // this captured as upvalue (closure inside a method)
    int upvalue = resolve_upvalue("this");
    if (upvalue >= 0) {
        emit(op_byte(Opcode::GET_UPVALUE), reg, upvalue, 0);
        return;
    }
    emit(op_byte(Opcode::LOAD_NIL), reg, 0, 0);
}

void CodeGenerator::compile_super(SuperAccessExpr*, int reg) {
    // Simplified: just load nil for now
    emit(op_byte(Opcode::LOAD_NIL), reg, 0, 0);
}

// Statements
void CodeGenerator::compile_expr_stmt(ExprStmt* node) {
    compile_expr(node->expression);
    free_register(); // result not needed
}

void CodeGenerator::compile_block(BlockStmt* node) {
    begin_scope();
    for (auto& stmt : node->statements) {
        compile_stmt(stmt);
    }
    end_scope();
}

void CodeGenerator::compile_if(IfStmt* node) {
    int cond = compile_expr(node->condition);
    size_t then_jump = emit_jump(op_byte(Opcode::JMP_IF_FALSE), cond, 0);
    free_register(); // cond — only read by JMP_IF_FALSE

    compile_stmt(node->then_branch);
    if (node->else_branch) {
        size_t else_jump = emit_jump(op_byte(Opcode::JMP), 0, 0);
        patch_jump(then_jump, static_cast<int16_t>(current_offset() - then_jump - INST_SIZE));
        compile_stmt(node->else_branch);
        // JMP does NOT advance IP first
        patch_jump(else_jump, static_cast<int16_t>(current_offset() - else_jump));
    } else {
        patch_jump(then_jump, static_cast<int16_t>(current_offset() - then_jump - INST_SIZE));
    }
}

void CodeGenerator::compile_while(WhileStmt* node) {
    size_t loop_start = current_offset();
    int cond = compile_expr(node->condition);
    size_t exit_jump = emit_jump(op_byte(Opcode::JMP_IF_FALSE), cond, 0);
    free_register(); // cond — only read by JMP_IF_FALSE

    // Save break/continue targets
    auto& breaks = current_scope_->break_jumps;
    auto& continues = current_scope_->continue_targets;
    size_t old_break_size = breaks.size();
    size_t old_continue_size = continues.size();

    compile_stmt(node->body);

    // Jump back to condition
    // JMP does NOT advance IP before applying offset (unlike JMP_IF_FALSE),
    // so the offset is loop_start - current (no -1).
    int16_t back_offset = static_cast<int16_t>(loop_start - current_offset());
    emit_bx(op_byte(Opcode::JMP), 0, static_cast<uint16_t>(back_offset));

    patch_jump(exit_jump, static_cast<int16_t>(current_offset() - exit_jump - INST_SIZE));

    // Patch breaks to point here (JMP does NOT advance IP first)
    for (size_t i = old_break_size; i < breaks.size(); i++) {
        patch_jump(breaks[i], static_cast<int16_t>(current_offset() - breaks[i]));
    }
    breaks.resize(old_break_size);

    // Patch continues to point to loop start
    for (size_t i = old_continue_size; i < continues.size(); i++) {
        int16_t cont_offset = static_cast<int16_t>(loop_start - continues[i]);
        patch_jump(continues[i], cont_offset);
    }
    continues.resize(old_continue_size);
}

void CodeGenerator::compile_for(ForStmt* node) {
    begin_scope();
    if (node->init) compile_stmt(node->init);

    size_t loop_start = current_offset();
    size_t exit_jump = 0;
    if (node->cond) {
        int cond = compile_expr(node->cond);
        exit_jump = emit_jump(op_byte(Opcode::JMP_IF_FALSE), cond, 0);
        free_register(); // cond — only read by JMP_IF_FALSE
    }

    auto& breaks = current_scope_->break_jumps;
    auto& continues = current_scope_->continue_targets;
    size_t old_break_size = breaks.size();
    size_t old_continue_size = continues.size();

    compile_stmt(node->body);

    // Continue target: the update expression
    size_t continue_target = current_offset();
    if (node->update) {
        compile_expr(node->update);
        free_register();
    }

    // JMP does NOT advance IP before applying offset
    int16_t back_offset = static_cast<int16_t>(loop_start - current_offset());
    emit_bx(op_byte(Opcode::JMP), 0, static_cast<uint16_t>(back_offset));

    if (exit_jump > 0) {
        patch_jump(exit_jump, static_cast<int16_t>(current_offset() - exit_jump - INST_SIZE));
    }

    for (size_t i = old_break_size; i < breaks.size(); i++) {
        patch_jump(breaks[i], static_cast<int16_t>(current_offset() - breaks[i]));
    }
    breaks.resize(old_break_size);

    for (size_t i = old_continue_size; i < continues.size(); i++) {
        int16_t cont_offset = static_cast<int16_t>(continue_target - continues[i]);
        patch_jump(continues[i], cont_offset);
    }
    continues.resize(old_continue_size);

    end_scope();
}

void CodeGenerator::compile_for_in(ForInStmt* node) {
    begin_scope();
    // Compile iterable
    int iter_reg = compile_expr(node->iterable);
    // Create iterator
    int iter_obj = alloc_register();
    emit(op_byte(Opcode::ITER_INIT), iter_obj, iter_reg, 0);

    // Declare loop variable
    int var_reg = alloc_register();
    declare_local(node->variable, var_reg);

    size_t loop_start = current_offset();

    // Get next value
    emit(op_byte(Opcode::ITER_NEXT), var_reg, iter_obj, 0);

    // Check if done
    int done_reg = alloc_register();
    emit(op_byte(Opcode::ITER_DONE), done_reg, iter_obj, 0);
    size_t exit_jump = emit_jump(op_byte(Opcode::JMP_IF_TRUE), done_reg, 0);

    auto& breaks = current_scope_->break_jumps;
    auto& continues = current_scope_->continue_targets;
    size_t old_break_size = breaks.size();
    size_t old_continue_size = continues.size();

    compile_stmt(node->body);

    // Continue target
    // JMP does NOT advance IP before applying offset
    int16_t back_offset = static_cast<int16_t>(loop_start - current_offset());
    emit_bx(op_byte(Opcode::JMP), 0, static_cast<uint16_t>(back_offset));

    patch_jump(exit_jump, static_cast<int16_t>(current_offset() - exit_jump - INST_SIZE));

    for (size_t i = old_break_size; i < breaks.size(); i++) {
        patch_jump(breaks[i], static_cast<int16_t>(current_offset() - breaks[i]));
    }
    breaks.resize(old_break_size);

    for (size_t i = old_continue_size; i < continues.size(); i++) {
        int16_t cont_offset = static_cast<int16_t>(loop_start - continues[i]);
        patch_jump(continues[i], cont_offset);
    }
    continues.resize(old_continue_size);

    // Free registers in reverse allocation order: done_reg, then end_scope frees var_reg,
    // then iter_obj, then iter_reg (from compile_expr)
    free_register(); // done_reg
    end_scope(); // frees var_reg (declared as local)
    free_register(); // iter_obj
    free_register(); // iter_reg
}

void CodeGenerator::compile_break(BreakStmt*) {
    current_scope_->break_jumps.push_back(emit_jump(op_byte(Opcode::JMP), 0, 0));
}

void CodeGenerator::compile_continue(ContinueStmt*) {
    current_scope_->continue_targets.push_back(emit_jump(op_byte(Opcode::JMP), 0, 0));
}

void CodeGenerator::compile_return(ReturnStmt* node) {
    // Close captured locals before returning
    auto close_captured = [&]() {
        for (auto& local : current_scope_->locals) {
            if (local.is_captured) {
                emit(op_byte(Opcode::CLOSE_UPVALUE), local.register_id, 0, 0);
            }
        }
    };

    if (node->values.empty()) {
        // return;
        int reg = alloc_register();
        emit(op_byte(Opcode::LOAD_NIL), reg, 0, 0);
        close_captured();
        emit(op_byte(Opcode::RETURN), reg, 0, 0);
        free_register();
    } else if (node->values.size() == 1) {
        // Single return — try tail call optimization
        if (node->values[0]->type == NodeType::Call) {
            auto* call_node = static_cast<CallExpr*>(node->values[0].get());
            if (call_node->callee->type != NodeType::FieldAccess) {
                int callee = compile_expr(call_node->callee);
                std::vector<int> arg_regs;
                for (auto& arg : call_node->arguments) {
                    arg_regs.push_back(compile_expr(arg));
                }
                // Move args left-to-right to avoid clobbering source registers
                for (int i = 0; i < static_cast<int>(arg_regs.size()); i++) {
                    int target = callee + 1 + i;
                    if (arg_regs[i] != target) {
                        emit(op_byte(Opcode::MOVE), target, arg_regs[i], 0);
                    }
                }
                close_captured();
                emit(op_byte(Opcode::TAIL_CALL), callee, static_cast<int>(arg_regs.size()), 0);
                return;
            }
        }
        int reg = compile_expr(node->values[0]);
        close_captured();
        emit(op_byte(Opcode::RETURN), reg, 0, 0);
    } else {
        // Multi-return: return a, b, c → pack into array and return
        // First collect all value registers
        std::vector<int> val_regs;
        for (auto& val : node->values) {
            val_regs.push_back(compile_expr(val));
        }
        // Build array from values
        int arr_reg = alloc_register();
        emit(op_byte(Opcode::NEW_ARRAY), arr_reg, static_cast<uint8_t>(val_regs.size()), 0);
        for (size_t i = 0; i < val_regs.size(); i++) {
            int idx_reg = alloc_register();
            uint16_t idx_const = make_constant(Value(static_cast<double>(i)));
            emit_bx(op_byte(Opcode::LOAD_CONST), idx_reg, idx_const);
            emit(op_byte(Opcode::SET_INDEX), arr_reg, idx_reg, val_regs[i]);
            free_register(); // idx_reg
        }
        close_captured();
        emit(op_byte(Opcode::RETURN), arr_reg, 0, 0);
    }
}

void CodeGenerator::compile_let(LetStmt* node) {
    if (node->initializer) {
        int init_reg = compile_expr(node->initializer);
        if (current_scope_->enclosing == nullptr && current_scope_->scope_depth == 0) {
            // Top-level: set as global
            uint16_t name_const = make_identifier_constant(node->name);
            emit_bx(op_byte(Opcode::SET_GLOBAL), init_reg, name_const);
            free_register(); // free the init_reg
        } else {
            // init_reg is already allocated by compile_expr, use it directly as the local
            declare_local(node->name, init_reg);
        }
    } else {
        int reg = alloc_register();
        emit(op_byte(Opcode::LOAD_NIL), reg, 0, 0);
        if (current_scope_->enclosing == nullptr && current_scope_->scope_depth == 0) {
            uint16_t name_const = make_identifier_constant(node->name);
            emit_bx(op_byte(Opcode::SET_GLOBAL), reg, name_const);
            free_register();
        } else {
            declare_local(node->name, reg);
        }
    }
}

void CodeGenerator::compile_fn(FnStmt* node) {
    // For named local functions, declare the name first so recursive
    // references inside the body can resolve it as a local.
    int reg = -1;
    bool is_local_named = !node->name.empty() &&
        !(current_scope_->enclosing == nullptr && current_scope_->scope_depth == 0);
    if (is_local_named) {
        reg = alloc_register();
        emit(op_byte(Opcode::LOAD_NIL), reg, 0, 0); // placeholder
        declare_local(node->name, reg);
    }

    auto* func = compile_function(node->name, node->params, node->body);
    uint16_t func_const = make_constant(Value(static_cast<Obj*>(func)));

    if (is_local_named) {
        // Reuse the already-allocated register for the closure
        emit_bx(op_byte(Opcode::CLOSURE), reg, func_const);
    } else {
        reg = alloc_register();
        emit_bx(op_byte(Opcode::CLOSURE), reg, func_const);
    }

    if (!node->name.empty()) {
        // At top level, set as global
        if (current_scope_->enclosing == nullptr && current_scope_->scope_depth == 0) {
            uint16_t name_const = make_identifier_constant(node->name);
            emit_bx(op_byte(Opcode::SET_GLOBAL), reg, name_const);
            free_register();
        }
        // else: already declared as local above
    }
}

void CodeGenerator::compile_class(ClassStmt* node) {
    uint16_t name_const = make_identifier_constant(node->name);
    int class_reg = alloc_register();
    emit_bx(op_byte(Opcode::NEW_CLASS), class_reg, name_const);

    // Add methods
    for (auto& method : node->methods) {
        std::vector<Param> method_params;
        method_params.push_back({"this"});  // "this" occupies register 0
        for (auto& p : method->params) {
            method_params.push_back(p);
        }
        // Arity is the user-visible parameter count (not including this)
        auto* func = compile_function(method->name, method_params, method->body);
        uint16_t func_const = make_constant(Value(static_cast<Obj*>(func)));
        int method_reg = alloc_register();
        emit_bx(op_byte(Opcode::CLOSURE), method_reg, func_const);
        uint16_t method_const = make_identifier_constant(method->name);
        emit(op_byte(Opcode::SET_FIELD), class_reg, method_const, method_reg);
        free_register();
    }

    // At top level, set as global
    if (current_scope_->enclosing == nullptr && current_scope_->scope_depth == 0) {
        uint16_t name_const2 = make_identifier_constant(node->name);
        emit_bx(op_byte(Opcode::SET_GLOBAL), class_reg, name_const2);
        free_register(); // class_reg no longer needed for global path
    } else {
        declare_local(node->name, class_reg);
    }
}

void CodeGenerator::compile_await(AwaitStmt* node) {
    int reg = compile_expr(node->expr);
    emit(op_byte(Opcode::AWAIT), reg, 0, 0);
    free_register();
}

void CodeGenerator::compile_destructuring(DestructuringStmt* node) {
    int init_reg = compile_expr(node->initializer);

    // Get array length once (allocate AFTER init_reg to keep LIFO order)
    int len_reg = alloc_register();
    emit(op_byte(Opcode::GET_FIELD), len_reg, init_reg,
         make_identifier_constant("length"));

    for (size_t i = 0; i < node->names.size(); i++) {
        // Allocate register for the value (will become a local)
        int val_reg = alloc_register();

        // Default to nil (covers out-of-bounds case)
        emit(op_byte(Opcode::LOAD_NIL), val_reg, 0, 0);

        // Bounds check: if i < length, overwrite with arr[i]
        int idx_reg = alloc_register();
        uint16_t idx_const = make_constant(Value(static_cast<double>(i)));
        emit_bx(op_byte(Opcode::LOAD_CONST), idx_reg, idx_const);
        int cmp_reg = alloc_register();
        emit(op_byte(Opcode::LT), cmp_reg, idx_reg, len_reg);
        size_t skip_jump = emit_jump(op_byte(Opcode::JMP_IF_FALSE), cmp_reg, 0);
        // In bounds: GET_INDEX overwrites val_reg
        emit(op_byte(Opcode::GET_INDEX), val_reg, init_reg, idx_reg);
        patch_jump(skip_jump, static_cast<int16_t>(current_offset() - skip_jump - INST_SIZE));
        free_register(); // cmp_reg
        free_register(); // idx_reg

        // Declare the name (val_reg stays allocated as a local)
        if (current_scope_->enclosing == nullptr && current_scope_->scope_depth == 0) {
            uint16_t name_const = make_identifier_constant(node->names[i]);
            emit_bx(op_byte(Opcode::SET_GLOBAL), val_reg, name_const);
            free_register(); // val_reg no longer needed for global path
        } else {
            declare_local(node->names[i], val_reg);
            // val_reg is now owned by the local, don't free
        }
    }
    // Note: len_reg and init_reg are NOT freed here because locals were
    // allocated at higher registers. They're accounted for in max_registers.
    (void)len_reg;
    (void)init_reg;
}

void CodeGenerator::compile_switch(SwitchStmt* node) {
    int cond_reg = compile_expr(node->expr);
    std::vector<size_t> end_jumps;

    for (auto& clause : node->cases) {
        // Check each value in the case (supports case a, b, c:)
        std::vector<size_t> value_matches;
        for (auto& val : clause.values) {
            int val_reg = compile_expr(val);
            int eq_reg = alloc_register();
            emit(op_byte(Opcode::EQ), eq_reg, cond_reg, val_reg);
            free_register(); // val_reg
            size_t jmp = emit_jump(op_byte(Opcode::JMP_IF_TRUE), eq_reg, 0);
            value_matches.push_back(jmp);
            free_register(); // eq_reg
        }

        // None matched — jump to next case
        size_t fallthrough_jump = emit_jump(op_byte(Opcode::JMP), 0, 0);

        // Patch value matches to jump here (the body)
        for (auto& jmp : value_matches) {
            patch_jump(jmp, static_cast<int16_t>(current_offset() - jmp - INST_SIZE));
        }

        // Compile body
        if (clause.body->type == NodeType::Block) {
            auto* block = static_cast<BlockStmt*>(clause.body.get());
            for (auto& stmt : block->statements) compile_stmt(stmt);
        } else {
            compile_stmt(clause.body);
        }

        // Jump to end (no fallthrough)
        end_jumps.push_back(emit_jump(op_byte(Opcode::JMP), 0, 0));

        // Patch fallthrough to here (JMP does NOT advance IP first)
        patch_jump(fallthrough_jump, static_cast<int16_t>(current_offset() - fallthrough_jump));
    }

    // Default body
    if (node->default_body) {
        if (node->default_body->type == NodeType::Block) {
            auto* block = static_cast<BlockStmt*>(node->default_body.get());
            for (auto& stmt : block->statements) compile_stmt(stmt);
        } else {
            compile_stmt(node->default_body);
        }
    }

    // Patch all end jumps (JMP does NOT advance IP first)
    for (auto& jmp : end_jumps) {
        patch_jump(jmp, static_cast<int16_t>(current_offset() - jmp));
    }

    // Patch break jumps to end of switch (break in switch = jump to end)
    for (auto& jmp : current_scope_->break_jumps) {
        patch_jump(jmp, static_cast<int16_t>(current_offset() - jmp));
    }
    current_scope_->break_jumps.clear();

    free_register(); // cond_reg
}

void CodeGenerator::compile_try_catch(TryCatchStmt* node) {
    // Reserve register for catch variable first
    int err_reg = alloc_register();

    // TRY_BEGIN — BX will be patched to catch offset, A = register for exception
    size_t try_begin_idx = emit_jump(op_byte(Opcode::TRY_BEGIN), err_reg, 0);

    // Compile try body
    if (node->try_body->type == NodeType::Block) {
        auto* block = static_cast<BlockStmt*>(node->try_body.get());
        for (auto& stmt : block->statements) compile_stmt(stmt);
    } else {
        compile_stmt(node->try_body);
    }

    // TRY_END — skip catch if no error
    size_t try_end_idx = emit_jump(op_byte(Opcode::TRY_END), 0, 0);

    // Catch body starts here — patch TRY_BEGIN to jump here
    patch_jump(try_begin_idx, static_cast<int16_t>(current_offset() - try_begin_idx - INST_SIZE));

    // Bind catch variable
    if (current_scope_->enclosing == nullptr && current_scope_->scope_depth == 0) {
        uint16_t name_const = make_identifier_constant(node->catch_var);
        emit_bx(op_byte(Opcode::SET_GLOBAL), err_reg, name_const);
        free_register(); // err_reg no longer needed for global path
    } else {
        declare_local(node->catch_var, err_reg);
    }

    // Compile catch body
    if (node->catch_body->type == NodeType::Block) {
        auto* block = static_cast<BlockStmt*>(node->catch_body.get());
        for (auto& stmt : block->statements) compile_stmt(stmt);
    } else {
        compile_stmt(node->catch_body);
    }

    // Patch TRY_END to jump past catch
    patch_jump(try_end_idx, static_cast<int16_t>(current_offset() - try_end_idx - INST_SIZE));
}

void CodeGenerator::compile_throw(ThrowStmt* node) {
    int reg = compile_expr(node->value);
    emit(op_byte(Opcode::THROW), reg, 0, 0);
    free_register();
}

void CodeGenerator::compile_include(IncludeStmt* node) {
    // Resolve path relative to base_path_
    std::string resolved_path = node->path;
    if (!base_path_.empty() && node->path[0] != '/') {
        resolved_path = base_path_ + "/" + node->path;
    }

    // Prevent circular includes
    if (visited_files_.count(resolved_path)) {
        return; // Already included, skip
    }
    visited_files_.insert(resolved_path);

    // Read the file
    std::ifstream file(resolved_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open included file: " + resolved_path);
    }
    std::stringstream ss;
    ss << file.rdbuf();
    std::string source = ss.str();
    file.close();

    // Lex and parse
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    ASTPtr included_ast;
    try {
        included_ast = parser.parse_program();
    } catch (const std::exception& e) {
        throw std::runtime_error("Parse error in included file '" + resolved_path + "': " + e.what());
    }

    // Set base path for nested includes (directory of the included file)
    std::string old_base = base_path_;
    size_t last_slash = resolved_path.rfind('/');
    if (last_slash != std::string::npos) {
        base_path_ = resolved_path.substr(0, last_slash);
    }

    // Compile the included AST inline
    if (included_ast->type == NodeType::Block) {
        auto* block = static_cast<BlockStmt*>(included_ast.get());
        for (auto& stmt : block->statements) {
            compile_stmt(stmt);
        }
    } else {
        compile_stmt(included_ast);
    }

    // Restore base path
    base_path_ = old_base;
}

// Register allocation
int CodeGenerator::alloc_register() {
    int reg = current_scope_->next_register++;
    if (reg >= 65535) throw std::runtime_error("Register limit exceeded");
    if (current_scope_->next_register > current_scope_->max_registers) {
        current_scope_->max_registers = current_scope_->next_register;
    }
    return reg;
}

void CodeGenerator::free_register() {
    if (current_scope_->next_register > 0) current_scope_->next_register--;
}

int CodeGenerator::current_register() const {
    return current_scope_->next_register - 1;
}

// Scope management
void CodeGenerator::begin_scope() {
    current_scope_->scope_depth++;
    signal_scope_stack_.push_back({}); // track signals declared in this scope
}

void CodeGenerator::end_scope() {
    current_scope_->scope_depth--;
    // Remove locals at this depth
    auto& locals = current_scope_->locals;
    while (!locals.empty() && locals.back().depth > current_scope_->scope_depth) {
        if (locals.back().is_captured) {
            emit(op_byte(Opcode::CLOSE_UPVALUE), locals.back().register_id, 0, 0);
        }
        locals.pop_back();
        free_register();
    }
    // Remove signals declared in this scope from signal_set_
    if (!signal_scope_stack_.empty()) {
        for (auto& name : signal_scope_stack_.back()) {
            signal_set_.erase(name);
        }
        signal_scope_stack_.pop_back();
    }
}

void CodeGenerator::declare_local(const std::string& name, int reg) {
    current_scope_->locals.push_back({name, reg, current_scope_->scope_depth, false});
}

int CodeGenerator::resolve_local(const std::string& name) const {
    auto& locals = current_scope_->locals;
    for (int i = locals.size() - 1; i >= 0; i--) {
        if (locals[i].name == name) return locals[i].register_id;
    }
    return -1;
}

int CodeGenerator::resolve_upvalue(const std::string& name) {
    if (!current_scope_->enclosing) return -1;

    // Look in enclosing scope's locals
    auto& enclosing_locals = current_scope_->enclosing->locals;
    for (int i = enclosing_locals.size() - 1; i >= 0; i--) {
        if (enclosing_locals[i].name == name) {
            enclosing_locals[i].is_captured = true;
            return add_upvalue(enclosing_locals[i].register_id, true);
        }
    }

    // Recursively look in enclosing scope's upvalues (indirect capture)
    // Temporarily switch to enclosing scope and recurse
    CompilerScope* saved = current_scope_;
    current_scope_ = saved->enclosing;
    int upvalue = resolve_upvalue(name);
    current_scope_ = saved;

    if (upvalue >= 0) {
        return add_upvalue(static_cast<uint8_t>(upvalue), false);
    }

    return -1;
}

int CodeGenerator::add_upvalue(uint8_t index, bool is_local) {
    // Check if this upvalue already exists (deduplicate)
    auto& upvalues = current_scope_->upvalues;
    for (size_t i = 0; i < upvalues.size(); i++) {
        if (upvalues[i].index == index && upvalues[i].is_local == is_local) {
            return static_cast<int>(i);
        }
    }
    upvalues.push_back({index, is_local});
    return static_cast<int>(upvalues.size() - 1);
}

// Emission helpers
void CodeGenerator::emit(uint8_t op, int a, int b, int c) {
    // Auto-emit wide if any register exceeds 8-bit range
    if (a > 255 || b > 255 || c > 255) {
        emit_wide(op, a, b, c);
        return;
    }
    uint32_t inst = make_instruction(static_cast<Opcode>(op), a, b, c);
    current_scope_->function->bytecode.push_back(static_cast<uint8_t>(inst >> 24));
    current_scope_->function->bytecode.push_back(static_cast<uint8_t>((inst >> 16) & 0xFF));
    current_scope_->function->bytecode.push_back(static_cast<uint8_t>((inst >> 8) & 0xFF));
    current_scope_->function->bytecode.push_back(static_cast<uint8_t>(inst & 0xFF));
}

void CodeGenerator::emit_bx(uint8_t op, int a, uint16_t bx) {
    // Auto-emit wide if A exceeds 8-bit range, or if next_register > 255 and BX > 255
    // (normal emit truncates B/C to 8 bits, losing the BX high byte)
    if (a > 255 || (current_scope_->next_register > 255 && bx > 255)) {
        emit_wide(op, a, (bx >> 8) & 0xFF, bx & 0xFF);
        return;
    }
    uint32_t inst = make_instruction_bx(static_cast<Opcode>(op), a, bx);
    current_scope_->function->bytecode.push_back(static_cast<uint8_t>(inst >> 24));
    current_scope_->function->bytecode.push_back(static_cast<uint8_t>((inst >> 16) & 0xFF));
    current_scope_->function->bytecode.push_back(static_cast<uint8_t>((inst >> 8) & 0xFF));
    current_scope_->function->bytecode.push_back(static_cast<uint8_t>(inst & 0xFF));
}

void CodeGenerator::emit_wide(uint8_t op, int a, int b, int c) {
    auto& bc = current_scope_->function->bytecode;
    // WIDE prefix
    bc.push_back(op_byte(Opcode::WIDE));
    // Wide instruction: [op:8][A:16][B:16][C:16] = 7 bytes
    uint64_t inst = make_wide_instruction(static_cast<Opcode>(op), a, b, c);
    bc.push_back(static_cast<uint8_t>((inst >> 48) & 0xFF));
    bc.push_back(static_cast<uint8_t>((inst >> 40) & 0xFF));
    bc.push_back(static_cast<uint8_t>((inst >> 32) & 0xFF));
    bc.push_back(static_cast<uint8_t>((inst >> 24) & 0xFF));
    bc.push_back(static_cast<uint8_t>((inst >> 16) & 0xFF));
    bc.push_back(static_cast<uint8_t>((inst >> 8) & 0xFF));
    bc.push_back(static_cast<uint8_t>(inst & 0xFF));
}

size_t CodeGenerator::emit_jump(uint8_t op, int a, int16_t offset) {
    size_t byte_pos = current_scope_->function->bytecode.size();
    emit_bx(op, a, static_cast<uint16_t>(offset));
    return byte_pos;
}

void CodeGenerator::patch_jump(size_t jump_byte_pos, int16_t offset) {
    auto& bc = current_scope_->function->bytecode;
    // Callers compute offset = current_offset - jump_byte_pos - INST_SIZE (4)
    // For WIDE instructions, the correct offset is current_offset - jump_byte_pos - WIDE_INST_SIZE (8)
    // So we need to subtract (WIDE_INST_SIZE - INST_SIZE) = 4 from the offset
    if (bc[jump_byte_pos] == op_byte(Opcode::WIDE)) {
        offset = static_cast<int16_t>(offset - (WIDE_INST_SIZE - INST_SIZE));
    }
    uint16_t uoffset = static_cast<uint16_t>(offset);
    if (bc[jump_byte_pos] == op_byte(Opcode::WIDE)) {
        // Wide format: [WIDE][op][A:16][B:16][C:16] = 8 bytes
        // BX is split: B gets high byte, C gets low byte (matching emit_bx behavior)
        // Byte 4: B_high, Byte 5: B_low, Byte 6: C_high, Byte 7: C_low
        bc[jump_byte_pos + 4] = 0;
        bc[jump_byte_pos + 5] = static_cast<uint8_t>((uoffset >> 8) & 0xFF);
        bc[jump_byte_pos + 6] = 0;
        bc[jump_byte_pos + 7] = static_cast<uint8_t>(uoffset & 0xFF);
    } else {
        // Normal format: [op][A][B][C] = 4 bytes
        // BX offset is in B:8|C:8 = bytes 2-3
        bc[jump_byte_pos + 2] = static_cast<uint8_t>((uoffset >> 8) & 0xFF);
        bc[jump_byte_pos + 3] = static_cast<uint8_t>(uoffset & 0xFF);
    }
}

size_t CodeGenerator::current_offset() const {
    return current_scope_->function->bytecode.size(); // byte position
}

size_t CodeGenerator::add_constant(Value value) {
    // Deduplicate constants to save space and delay 65K limit
    auto& constants = current_scope_->function->constants;
    for (size_t i = 0; i < constants.size(); i++) {
        if (constants[i] == value) return i;
    }
    constants.push_back(value);
    return constants.size() - 1;
}

uint16_t CodeGenerator::make_constant(Value value) {
    size_t idx = add_constant(value);
    if (idx > 65535) throw std::runtime_error("Too many constants");
    return static_cast<uint16_t>(idx);
}

uint16_t CodeGenerator::make_identifier_constant(const std::string& name) {
    auto* str = get_string_table().intern(name);
    uint16_t idx = make_constant(Value(static_cast<Obj*>(str)));
    identifier_values_.insert(name);
    return idx;
}

void CodeGenerator::compile_signal_decl(SignalDeclStmt* node) {
    // Compile the initial value — reuse its register for the signal object
    // SIGNAL_CREATE A, B reads S(B) first, then writes to S(A), so A==B is safe
    int reg = compile_expr(node->initializer);
    emit(op_byte(Opcode::SIGNAL_CREATE), reg, reg, 0);

    // Track this variable as a signal
    signal_set_.insert(node->name);

    if (current_scope_->enclosing == nullptr && current_scope_->scope_depth == 0) {
        // Top-level: set as global (signal stays in signal_set_ permanently)
        uint16_t name_const = make_identifier_constant(node->name);
        emit_bx(op_byte(Opcode::SET_GLOBAL), reg, name_const);
        free_register();
    } else {
        // Local signal: register for scope cleanup
        if (!signal_scope_stack_.empty()) {
            signal_scope_stack_.back().push_back(node->name);
        }
        // reg is already allocated by compile_expr, use it directly as the local
        declare_local(node->name, reg);
    }
}

void CodeGenerator::compile_effect(EffectStmt* node) {
    // The effect body becomes a closure (function with no args)
    auto* func = compile_function("__effect__", {}, node->body);
    uint16_t func_const = make_constant(Value(static_cast<Obj*>(func)));

    // Create closure
    int closure_reg = alloc_register();
    emit_bx(op_byte(Opcode::CLOSURE), closure_reg, func_const);

    // Create effect from closure: EFFECT_CREATE dest, closure_reg
    int effect_reg = alloc_register();
    emit(op_byte(Opcode::EFFECT_CREATE), effect_reg, closure_reg, 0);
    free_register(); // closure_reg

    // Run the effect immediately (also registers dependencies)
    emit(op_byte(Opcode::EFFECT_RUN), effect_reg, 0, 0);
    free_register(); // effect_reg
}

void CodeGenerator::compile_enum(EnumStmt* node) {
    // Assign a type ID to this enum
    uint16_t type_id = enum_type_id_counter_++;
    enum_type_ids_[node->name] = type_id;

    // Create a class to hold enum variants
    uint16_t name_const = make_identifier_constant(node->name);
    int class_reg = alloc_register();
    emit_bx(op_byte(Opcode::ENUM_CREATE), class_reg, name_const);

    // Store type_id as a constant for later use
    uint16_t type_id_const = make_constant(Value(static_cast<double>(type_id)));

    // Register each variant
    int vi = 0;
    for (auto& variant : node->variants) {
        uint16_t variant_name_const = make_identifier_constant(variant.name);

        if (variant.value) {
            // Data-carrying variant: creates a factory method on the class
            // For now, we store the variant index and let the factory create instances
            // ENUM_DATA_VARIANT class_reg, variant_name_const
            emit(op_byte(Opcode::ENUM_DATA_VARIANT), class_reg, variant_name_const, 0);
        } else {
            // Simple variant: NaN-boxed immediate value
            // Create the enum value constant
            int val_reg = alloc_register();
            // Load type_id and variant_index as constants, then the VM will create the enum value
            // We use ENUM_VARIANT: class_reg, name_const, value_const
            // The value is the variant index encoded as a number (VM will convert to NaN-boxed enum)
            uint16_t vi_const = make_constant(Value(static_cast<double>(vi)));
            emit(op_byte(Opcode::ENUM_VARIANT), class_reg, variant_name_const, vi_const);
            free_register(); // val_reg
        }
        vi++;
    }

    // At top level, set as global
    if (current_scope_->enclosing == nullptr && current_scope_->scope_depth == 0) {
        uint16_t name_const2 = make_identifier_constant(node->name);
        emit_bx(op_byte(Opcode::SET_GLOBAL), class_reg, name_const2);
        free_register(); // class_reg
    } else {
        declare_local(node->name, class_reg);
    }
}

// Returns the destination register (A operand) for an opcode, or -1 if it doesn't write to A.
static int opcode_dest_reg(uint8_t op) {
    // Opcodes that write to register A
    switch (static_cast<Opcode>(op)) {
        case Opcode::LOAD_CONST: case Opcode::LOAD_NIL: case Opcode::LOAD_TRUE:
        case Opcode::LOAD_FALSE: case Opcode::MOVE: case Opcode::GET_LOCAL:
        case Opcode::GET_UPVALUE: case Opcode::GET_GLOBAL:
        case Opcode::ADD: case Opcode::SUB: case Opcode::MUL: case Opcode::DIV:
        case Opcode::MOD: case Opcode::NEG:
        case Opcode::EQ: case Opcode::NEQ: case Opcode::LT: case Opcode::LTE:
        case Opcode::GT: case Opcode::GTE: case Opcode::NOT:
        case Opcode::CALL: case Opcode::CLOSURE: case Opcode::NEW_ARRAY:
        case Opcode::NEW_MAP: case Opcode::GET_INDEX: case Opcode::GET_FIELD:
        case Opcode::NEW_CLASS: case Opcode::NEW_INSTANCE: case Opcode::GET_METHOD:
        case Opcode::NEW_RANGE: case Opcode::ITER_INIT: case Opcode::ITER_NEXT:
        case Opcode::ITER_DONE:
        case Opcode::ADD_NUM: case Opcode::SUB_NUM: case Opcode::MUL_NUM:
        case Opcode::DIV_NUM: case Opcode::MOD_NUM: case Opcode::ADD_STR:
        case Opcode::EQ_NUM: case Opcode::NEQ_NUM: case Opcode::LT_NUM:
        case Opcode::LTE_NUM: case Opcode::GT_NUM: case Opcode::GTE_NUM:
        case Opcode::MOD_EQ_ZERO:
        case Opcode::SIGNAL_CREATE: case Opcode::SIGNAL_GET:
        case Opcode::EFFECT_CREATE:
        case Opcode::ENUM_CREATE: case Opcode::ENUM_GET: case Opcode::ENUM_IS:
        case Opcode::LOAD_IMM:
            return 0; // A is destination
        default:
            return -1; // doesn't write to A, or is a control flow op
    }
}

// Returns the set of source registers that an opcode reads from.
// Returns a bitmask: bit 0 = reads A, bit 1 = reads B, bit 2 = reads C.
static int opcode_src_regs(uint8_t op) {
    switch (static_cast<Opcode>(op)) {
        case Opcode::MOVE: return 2; // reads B only
        case Opcode::NEG: case Opcode::NOT:
        case Opcode::SIGNAL_GET: case Opcode::SIGNAL_CREATE:
        case Opcode::ITER_INIT: case Opcode::ITER_DONE:
        case Opcode::GET_UPVALUE: case Opcode::EFFECT_CREATE:
        case Opcode::NEW_INSTANCE:
            return 2; // reads B only
        case Opcode::SET_LOCAL: case Opcode::SET_UPVALUE: case Opcode::SET_GLOBAL:
        case Opcode::PRINT: case Opcode::RETURN:
        case Opcode::CLOSE_UPVALUE: case Opcode::THROW: case Opcode::AWAIT:
        case Opcode::JMP_IF_FALSE: case Opcode::JMP_IF_TRUE:
        case Opcode::FIBER_YIELD: case Opcode::EFFECT_RUN:
            return 1; // reads A only
        case Opcode::SIGNAL_SET:
            return 3; // reads A and B
        case Opcode::SET_FIELD:
            return 5; // reads A and C (B is constant index)
        case Opcode::ADD: case Opcode::SUB: case Opcode::MUL: case Opcode::DIV:
        case Opcode::MOD: case Opcode::EQ: case Opcode::NEQ:
        case Opcode::LT: case Opcode::LTE: case Opcode::GT: case Opcode::GTE:
        case Opcode::NEW_RANGE:
        case Opcode::ADD_NUM: case Opcode::SUB_NUM: case Opcode::MUL_NUM:
        case Opcode::DIV_NUM: case Opcode::MOD_NUM: case Opcode::ADD_STR:
        case Opcode::EQ_NUM: case Opcode::NEQ_NUM: case Opcode::LT_NUM:
        case Opcode::LTE_NUM: case Opcode::GT_NUM: case Opcode::GTE_NUM:
        case Opcode::MOD_EQ_ZERO:
        case Opcode::GET_FIELD: case Opcode::GET_INDEX: case Opcode::GET_METHOD:
        case Opcode::FIBER_RESUME: case Opcode::ENUM_IS:
            return 6; // reads B and C
        case Opcode::SET_INDEX:
            return 7; // reads A (object), B (index), and C (value)
        case Opcode::ENUM_VARIANT: case Opcode::ENUM_DATA_VARIANT:
            return 1; // reads A (class register)
        default:
            return 0;
    }
}

void CodeGenerator::peephole_optimize(std::vector<uint8_t>& bytecode) {
    size_t len = bytecode.size();

    // Pass 1: Dead MOVE elimination (forward scan)
    // A MOVE A, B is dead if A is overwritten before being read,
    // scanning forward until we hit a JMP or find a read of A.
    for (size_t i = 0; i + 4 <= len; i += 4) {
        uint8_t op = bytecode[i];
        if (op != op_byte(Opcode::MOVE)) continue;
        uint8_t move_dest = bytecode[i + 1]; // A operand

        // Scan forward from the next instruction
        bool dead = false;
        for (size_t j = i + 4; j + 4 <= len; j += 4) {
            uint8_t scan_op = bytecode[j];
            if (scan_op == op_byte(Opcode::WIDE)) break; // can't analyze wide
            // Control flow — can't determine, keep MOVE for safety
            if (scan_op == op_byte(Opcode::JMP_IF_FALSE) ||
                scan_op == op_byte(Opcode::JMP_IF_TRUE) ||
                scan_op == op_byte(Opcode::RETURN) ||
                scan_op == op_byte(Opcode::HALT) ||
                scan_op == op_byte(Opcode::CALL) ||
                scan_op == op_byte(Opcode::TAIL_CALL)) {
                break;
            }
            // Backward JMP (loop back-edge): check if jump target overwrites MOVE dest
            if (scan_op == op_byte(Opcode::JMP)) {
                int16_t jmp_offset = static_cast<int16_t>((bytecode[j + 2] << 8) | bytecode[j + 3]);
                if (jmp_offset < 0) {
                    size_t target = static_cast<size_t>(static_cast<int64_t>(j) + 4 + jmp_offset);
                    if (target + 4 <= len) {
                        // Check if the instruction at the jump target overwrites MOVE dest
                        uint8_t target_op = bytecode[target];
                        int target_dest = opcode_dest_reg(target_op);
                        if (target_dest >= 0 && bytecode[target + 1 + target_dest] == move_dest) {
                            dead = true;
                        }
                    }
                }
                break; // can't scan past any JMP
            }

            int dest = opcode_dest_reg(scan_op);
            int srcs = opcode_src_regs(scan_op);

            // Check if this instruction reads the MOVE destination (A, B, or C)
            if (srcs & 1) {
                if (bytecode[j + 1] == move_dest) break; // reads A
            }
            if (srcs & 2) {
                if (bytecode[j + 2] == move_dest) break; // reads B
            }
            if (srcs & 4) {
                if (bytecode[j + 3] == move_dest) break; // reads C
            }

            // Check if this instruction overwrites the MOVE destination
            if (dest >= 0 && bytecode[j + 1 + dest] == move_dest) {
                dead = true; // overwritten before being read — MOVE is dead
                break;
            }
        }

        if (dead) {
            bytecode[i] = op_byte(Opcode::NOP);
        }
    }

    // Pass 2: NOP compaction disabled — jump offset adjustment is complex
    // Dead MOVEs are replaced with NOPs which still dispatch but are very fast
    // (NOP handler just increments IP)
}

} // namespace akar
