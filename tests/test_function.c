#include "test.h"
#include <stdio.h>

static const char* source_code =
"int calc_number(int initial, int iterations)"
"{"
"   int i;"
"   i = initial;"
"loop:"
"   i = i * i - 1;"
"   x == 5 ? 9 : 1;"
"   if (i < iterations) {"
"       goto loop;"
"   }"
"   return i;"
"}";

int test_function(void)
{
    cc_parser parser;
    test_assert("source_code must be valid code", helper_create_parser(&parser, source_code));
    
    cc_ast_decl decl;
    test_assert("Function must be valid", cc_parser_parse_decl(&parser, &decl));
    
    print_ast_decl(&decl);

    test_assert("Function must have a body", decl.body);
    test_assert("Function must not be static", !decl.statik);
    test_assert("Function must be named 'calc_number'", !cc_token_strcmp(decl.name, CC_STR("calc_number")));

    const cc_ast_type* ret_type = decl.type->un.func.ret;
    test_assert("Return type must be int", ret_type->type_id == CC_AST_TYPEID_INT && ret_type->type_flags == 0);

    const cc_ast_decl_list* params = decl.type->un.func.params;
    const cc_ast_decl* initial = &params->decl;
    const cc_ast_decl* iterations = &params->next->decl;

    test_assert("1st parameter must be named 'initial'", !cc_token_strcmp(initial->name, CC_STR("initial")));
    test_assert("2nd parameter must be named 'iterations'", !cc_token_strcmp(iterations->name, CC_STR("iterations")));
    
    cc_parser_destroy(&parser);
    return 1;
}