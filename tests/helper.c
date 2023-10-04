#include "helper.h"
#include <cc/lexer.h>
#include <cc/parser.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

void test__assert(const char* comment, int result, const char* file, int line) {
    if (result)
        return;
    printf("Assertion failed: %s\nIn file \"%s:%d\"", comment, file, line);
    exit(1);
}

int helper_create_parser(cc_parser* out_parser, const char* source_code)
{
    cc_token* tokens;
    size_t num_tokens;

    if (!cc_lexer_readall(source_code, NULL, &tokens, &num_tokens))
        return 0;
    cc_parser_create(out_parser, tokens, tokens + num_tokens);
    return 1;
}

void print_ast_type(const cc_ast_type* t)
{
    switch (t->type_id)
    {
    case CC_AST_TYPEID_INT:     printf("int "); break;
    case CC_AST_TYPEID_CHAR:    printf("char "); break;
    case CC_AST_TYPEID_VOID:    printf("void "); break;
    case CC_AST_TYPEID_POINTER:
        print_ast_type(t->un.pointer);
        printf("* ");
        break;
    case CC_AST_TYPEID_TYPEDEF:
        printf("%.*s ", (int)cc_token_len(t->un.type_def), t->un.type_def->begin);
        break;
    case CC_AST_TYPEID_FUNCTION:
        print_ast_type(t->un.func.ret);
        printf("(");
        for (cc_ast_decl_list* next = t->un.func.params; next; next = next->next)
        {
            if (next != t->un.func.params)
                printf(", ");
            print_ast_decl(&next->decl);
        }
        printf(") ");
        break;
    default:
        printf("<unknown type-id> ");
    }

    if (t->type_flags)
    {
        if (t->type_flags & CC_AST_TYPEFLAG_CONST) printf("const ");
        if (t->type_flags & CC_AST_TYPEFLAG_VOLATILE) printf("volatile ");
        if (t->type_flags & CC_AST_TYPEFLAG_SHORT) printf("short ");
        if (t->type_flags & CC_AST_TYPEFLAG_LONG) printf("long ");
        if (t->type_flags & CC_AST_TYPEFLAG_LONGLONG) printf("long long ");
        if (t->type_flags & CC_AST_TYPEFLAG_SIGNED) printf("signed ");
        if (t->type_flags & CC_AST_TYPEFLAG_UNSIGNED) printf("unsigned ");
    }
}

void print_ast_decl(const cc_ast_decl* decl)
{
    print_ast_type(decl->type);
    printf("%.*s ", (int)cc_token_len(decl->name), decl->name->begin);
    if (decl->body)
        print_ast_body(decl->body);
}

void print_ast_const(const cc_ast_const* konst)
{
    switch (konst->constid)
    {
    case CC_AST_CONSTID_INT:
    case CC_AST_CONSTID_FLOAT:
    case CC_AST_CONSTID_STRING:
    case CC_AST_CONSTID_CHAR:
        printf("%.*s ", (int)cc_token_len(konst->token), konst->token->begin);
        break;
    default:
        printf("<unknown constid %d>", konst->constid);
    }
}

int print_ast_atomic(const cc_ast_expr* expr)
{
    switch (expr->exprid)
    {
    case CC_AST_EXPRID_CONST:
        print_ast_const(&expr->un.konst);
        break;
    case CC_AST_EXPRID_VARIABLE:
        printf("%.*s ", (int)cc_token_len(expr->un.variable), expr->un.variable->begin);
        break;
    default:
        return 0;
    }

    return 1;
}

int print_ast_unary(const cc_ast_expr* expr)
{
    switch (expr->exprid)
    {
    case CC_AST_EXPRID_REF:         printf("&"); break;
    case CC_AST_EXPRID_DEREF:       printf("*"); break;
    case CC_AST_EXPRID_CAST:        printf("(<cast>)"); break;
    case CC_AST_EXPRID_INC:         printf("++"); break;
    case CC_AST_EXPRID_DEC:         printf("--"); break;
    case CC_AST_SIZEOF:             printf("sizeof "); break;
    case CC_AST_EXPRID_BOOL_NOT:    printf("!"); break;
    case CC_AST_EXPRID_BIT_NOT:     printf("~"); break;
    default:
        return 0;
    }
    
    print_ast_expr(expr->un.unary.lhs);
    return 1;
}

int print_ast_binary(const cc_ast_expr* expr)
{
    const char* operator = NULL;

    switch (expr->exprid)
    {
    case CC_AST_EXPRID_COMMA:           operator = ", "; break;
    case CC_AST_EXPRID_ADD:             operator = "+ "; break;
    case CC_AST_EXPRID_SUB:             operator = "- "; break;
    case CC_AST_EXPRID_MUL:             operator = "* "; break;
    case CC_AST_EXPRID_DIV:             operator = "/ "; break;
    case CC_AST_EXPRID_MOD:             operator = "% "; break;
    case CC_AST_EXPRID_LSHIFT:          operator = "<< "; break;
    case CC_AST_EXPRID_RSHIFT:          operator = ">> "; break;
    case CC_AST_EXPRID_MEMBER:          operator = "."; break;
    case CC_AST_EXPRID_MEMBER_DEREF:    operator = "->"; break;
    case CC_AST_EXPRID_ASSIGN:          operator = "= "; break;
    case CC_AST_EXPRID_BOOL_OR:         operator = "|| "; break;
    case CC_AST_EXPRID_BOOL_AND:        operator = "&& "; break;
    case CC_AST_EXPRID_BIT_OR:          operator = "| "; break;
    case CC_AST_EXPRID_BIT_XOR:         operator = "^ "; break;
    case CC_AST_EXPRID_BIT_AND:         operator = "& "; break;
    case CC_AST_EXPRID_COMPARE_LT:      operator = "< "; break;
    case CC_AST_EXPRID_COMPARE_LTE:     operator = "<= "; break;
    case CC_AST_EXPRID_COMPARE_GT:      operator = "> "; break;
    case CC_AST_EXPRID_COMPARE_GTE:     operator = ">= "; break;
    case CC_AST_EXPRID_COMPARE_EQ:      operator = "== "; break;
    default:
        return 0;
    }
    
    printf("( ");
    print_ast_expr(expr->un.binary.lhs);
    printf("%s", operator);
    print_ast_expr(expr->un.binary.rhs);
    printf(") ");
    return 1;
}

int print_ast_ternary(const cc_ast_expr* expr)
{
    switch (expr->exprid)
    {
    case CC_AST_EXPRID_CONDITIONAL:
        print_ast_expr(expr->un.ternary.lhs);
        printf("? ");
        print_ast_expr(expr->un.ternary.middle);
        printf(": ");
        print_ast_expr(expr->un.ternary.rhs);
        break;
    default:
        return 0;
    }

    return 1;
}

void print_ast_expr(const cc_ast_expr* expr)
{
    if (print_ast_atomic(expr))
        ;
    else if (print_ast_unary(expr))
        ;
    else if (print_ast_binary(expr))
        ;
    else if (print_ast_ternary(expr))
        ;
    else
        printf("<unknown exprid %d>", expr->exprid);
}

void print_ast_stmt(const cc_ast_stmt* stmt)
{
    switch (stmt->stmtid)
    {
    case CC_AST_STMTID_RETURN: printf("return ");
    case CC_AST_STMTID_EXPR:
        print_ast_expr(&stmt->un.expr);
        printf("; ");
        break;
    case CC_AST_STMTID_DECL:
        print_ast_decl(&stmt->un.decl);
        printf("; ");
        break;
    case CC_AST_STMTID_IF:
        printf("if (");
        print_ast_expr(&stmt->un.if_.cond);
        printf(") ");
        print_ast_body(&stmt->un.if_.body);
        break;
    case CC_AST_STMTID_WHILE:
        printf("while (");
        print_ast_expr(&stmt->un.while_.cond);
        printf(") ");
        print_ast_body(&stmt->un.while_.body);
        break;
    case CC_AST_STMTID_DO_WHILE:
        printf("do ");
        print_ast_body(&stmt->un.dowhile_.body);
        printf("while (");
        print_ast_expr(&stmt->un.dowhile_.cond);
        printf(") ");
        break;
    case CC_AST_STMTID_FOR:
        printf("for (");
        print_ast_decl(&stmt->un.for_.start);
        printf("; ");
        print_ast_expr(&stmt->un.for_.cond);
        printf("; ");
        print_ast_expr(&stmt->un.for_.end);
        printf(") ");
        print_ast_body(&stmt->un.for_.body);
        break;
    case CC_AST_STMTID_GOTO:
        printf("goto %.*s; ", (int)cc_token_len(stmt->un.goto_), stmt->un.goto_->begin);
        break;
    case CC_AST_STMTID_LABEL:
        printf("%.*s: ", (int)cc_token_len(stmt->un.label), stmt->un.label->begin);
        break;
    case CC_AST_STMTID_BREAK: printf("break; "); break;
    case CC_AST_STMTID_CONTINUE: printf("continue; "); break;
    default:
        printf("<unknown stmtid %d>", stmt->stmtid);
    }
}

void print_ast_body(const cc_ast_body* body)
{
    printf("{\n");
    for (const cc_ast_stmt* next = body->stmt; next; next = next->next)
    {
        print_ast_stmt(next);
        printf("\n");
    }
    printf("} ");
}