#include "test.h"
#include <stdio.h>

const char* src_ternary = "x == 5 ? 9 : 0";

int test_expr(void)
{
    // ternary
    {
        cc_parser parse;
        test_assert("src_ternary must be valid code", helper_create_parser(&parse, src_ternary));

        cc_ast_expr expr;
        test_assert("src_ternary must be a valid expr", cc_parser_parse_expr(&parse, &expr));
        test_assert("expr must be a ternary conditional", expr.exprid == CC_AST_EXPRID_CONDITIONAL);
        
        const cc_ast_expr* lhs = expr.un.ternary.lhs;
        const cc_ast_expr* mid = expr.un.ternary.middle;
        const cc_ast_expr* rhs = expr.un.ternary.rhs;
        
        test_assert("lhs must be an equal-to comparison", lhs->exprid == CC_AST_EXPRID_COMPARE_EQ);
        test_assert("mid must be '9'", mid->exprid == CC_AST_EXPRID_CONST);
        test_assert("mid must be '9'", !cc_token_strcmp(mid->un.konst.token, CC_STR("9")));
        test_assert("rhs must be '0'", rhs->exprid == CC_AST_EXPRID_CONST);
        test_assert("rhs must be '0'", !cc_token_strcmp(rhs->un.konst.token, CC_STR("0")));

        cc_parser_destroy(&parse);
    }
    return 1;
}