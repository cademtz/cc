#include "test.h"
#include <stdio.h>

static const char* src_decl = "int i;";
static const char* src_assign = "i = 5;";
static const char* src_label = "loop:";
static const char* src_math = "i = i * i - 1;";
static const char* src_if =
    "if (i < 50) {"
    "    goto loop;"
    "}";
static const char* src_ret = "return i;";

int test_stmt(void)
{
    cc_parser parser;
    // decl
    {
        if (!helper_create_parser(&parser, src_decl))
            return 0;

        cc_ast_decl decl;
        test_assert("Code must be valid", cc_parser_parse_decl(&parser, &decl));
        printf("decl: ");
        print_ast_decl(&decl);
        printf("\n");

        test_assert("Decl must be an int", decl.type->type_id == CC_AST_TYPEID_INT);
        test_assert("Decl must be named 'i'", !cc_token_strcmp(decl.name, CC_STR("i")));
        test_assert("Decl must not have type flags", decl.type->type_flags == 0);
        cc_parser_destroy(&parser);
    }
    // assign
    {
        if (!helper_create_parser(&parser, src_assign))
            return 0;

        cc_ast_stmt stmt;
        test_assert("Code must be valid", cc_parser_parse_stmt(&parser, &stmt));
        
        printf("stmt: ");
        print_ast_stmt(&stmt);
        printf("\n");

        test_assert("Stmt must be an expr", stmt.stmtid == CC_AST_STMTID_EXPR);

        cc_ast_expr* expr = &stmt.un.expr;
        const cc_ast_expr* lhs = expr->un.binary.lhs;
        const cc_ast_expr* rhs = expr->un.binary.rhs;

        test_assert("Expr must be assignment", expr->exprid == CC_AST_EXPRID_ASSIGN);
        test_assert("Lhs must be 'i'", lhs->exprid == CC_AST_EXPRID_VARIABLE);
        test_assert("Lhs must be 'i'", !cc_token_strcmp(lhs->un.variable, CC_STR("i")));
        test_assert("Rhs must be 5", rhs->exprid == CC_AST_EXPRID_CONST);
        test_assert("Rhs must be 5", rhs->un.konst.constid == CC_AST_CONSTID_INT);
        test_assert("Rhs must be 5", !cc_token_strcmp(rhs->un.konst.token, CC_STR("5")));
        cc_parser_destroy(&parser);
    }
    // math
    {
        if (!helper_create_parser(&parser, src_math))
            return 0;

        cc_ast_stmt stmt;
        test_assert("Code must be valid", cc_parser_parse_stmt(&parser, &stmt));
        
        printf("stmt: ");
        print_ast_stmt(&stmt);
        printf("\n");

        test_assert("Stmt must be an expr", stmt.stmtid == CC_AST_STMTID_EXPR);

        cc_ast_expr* expr = &stmt.un.expr;
        const cc_ast_expr* lhs = expr->un.binary.lhs;
        const cc_ast_expr* rhs = expr->un.binary.rhs;
        
        test_assert("Expr must be assignment", expr->exprid == CC_AST_EXPRID_ASSIGN);
        test_assert("Lhs must be 'i'", lhs->exprid == CC_AST_EXPRID_VARIABLE);
        test_assert("Lhs must be 'i'", !cc_token_strcmp(lhs->un.variable, CC_STR("i")));
        test_assert("Rhs must be subtraction", rhs->exprid == CC_AST_EXPRID_SUB);

        // The subtraction expr `(i*i) - 1`
        const cc_ast_expr* sub = rhs;
        {
            const cc_ast_expr* lhs = sub->un.binary.lhs;
            const cc_ast_expr* rhs = sub->un.binary.rhs;
            test_assert("Lhs must be multiplication", lhs->exprid == CC_AST_EXPRID_MUL);
            test_assert("Rhs must be 1", rhs->exprid == CC_AST_EXPRID_CONST);
            test_assert("Rhs must be 1", !cc_token_strcmp(rhs->un.konst.token, CC_STR("1")));

            const cc_ast_expr* mul = lhs;
            {
                const cc_ast_expr* lhs = mul->un.binary.lhs;
                const cc_ast_expr* rhs = mul->un.binary.rhs;
                test_assert("Lhs must be 'i'", lhs->exprid == CC_AST_EXPRID_VARIABLE);
                test_assert("Lhs must be 'i'", !cc_token_strcmp(lhs->un.variable, CC_STR("i")));
                test_assert("Rhs must be 'i'", rhs->exprid == CC_AST_EXPRID_VARIABLE);
                test_assert("Rhs must be 'i'", !cc_token_strcmp(rhs->un.variable, CC_STR("i")));
            }
        }
        cc_parser_destroy(&parser);
    }
    return 1;
}
