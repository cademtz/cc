#pragma once
#include <cc/ast.h>
#include <cc/parser.h>
#include <cc/lib.h>
#include <assert.h>

/*
 * `comment` explains the expected behavior.
 * `expr` is an assertion.
 * Example: `test_assert("Type must be an int", decl.type == CC_AST_TYPEID_INT)`
 */
#define test_assert(comment, expr) test__assert(comment, (expr) != 0, __FILE__, __LINE__)
void test__assert(const char* comment, int result, const char* file, int line);

/**
 * @brief Create a parser for a given piece of source code
 * @param out_parser Location to store the parser
 * @param source_code The code to be parsed
 * @return 0 if the parser could not be created, likely due to unrecognized tokens
 */
int helper_create_parser(cc_parser* out_parser, const char* source_code);

struct cc_ir_func;

void print_ast_expr(const cc_ast_expr* expr);
void print_ast_body(const cc_ast_body* body);
void print_ast_decl(const cc_ast_decl* decl);
void print_ast_type(const cc_ast_type* t);
void print_ast_decl(const cc_ast_decl* decl);
void print_ast_const(const cc_ast_const* konst);
int print_ast_atomic(const cc_ast_expr* expr);
int print_ast_unary(const cc_ast_expr* expr);
int print_ast_binary(const cc_ast_expr* expr);
int print_ast_ternary(const cc_ast_expr* expr);
void print_ast_expr(const cc_ast_expr* expr);
void print_ast_stmt(const cc_ast_stmt* stmt);
void print_ast_body(const cc_ast_body* body);
void print_ir_func(const struct cc_ir_func* func);