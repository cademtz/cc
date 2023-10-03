#include "test.h"
#include <stdio.h>

static const char* source_code =
"int main()"
"{"
"   int i;"
"   i = 5;"
"loop:"
"   i = (i * i - 1);"
"   if (i < 50) {"
"       goto loop;"
"   }"
"   return i;"
"}";

int test_function(void)
{
    cc_parser parser;
    if (!helper_create_parser(&parser, source_code))
        return 0;
    
    cc_ast_decl c_decl;
    int ok = cc_parser_parse_decl(&parser, &c_decl);
    if (ok)
    {
        printf("Function: ");
        print_ast_decl(&c_decl);
        printf("\n");
    }
    else
        printf("Failed to parse\n");

    cc_parser_destroy(&parser);
    return 1;
}