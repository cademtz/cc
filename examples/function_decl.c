#include <stdio.h>
#include <cc/lexer.h>
#include <cc/parser.h>

static const char* source_code =
"int example(const char* text, int index) {"
"   return *(text + index) != 0;"
"}";

int main() {
    cc_token* tokens;
    size_t num_tokens;
    int err = 0;

    if (!cc_lexer_readall(source_code, NULL, &tokens, &num_tokens)) {
        printf("Unrecognized token in source_code\n");
        free(tokens);
        return 1;
    }

    cc_parser parser;
    cc_ast_decl decl; // AST node
    int parse_result;

    cc_parser_create(&parser, tokens, tokens + num_tokens);
    parse_result = cc_parser_parse_decl(&parser, &decl);

    if (parse_result) {
        printf("Function name: %.*s\n", (int)cc_token_len(decl.name), decl.name->begin);
        printf("Function body? %s\n", decl.body ? "true" : "false");
        printf("Static? %s\n", decl.statik ? "true" : "false");
    } else {
        printf("Failed to parse a declaration in source_code\n");
        err = 1;
    }

    cc_parser_destroy(&parser); // Every 'create' function has a 'destroy'
    free(tokens); // Free after use, according to function doc
    return err;
}