#pragma once
#include "lexer.h"
#include "ast.h"
#include "lib.h"

typedef struct cc_parser_savestate
{
    const cc_token* next;
    size_t num_allocs;
} cc_parser_savestate;

typedef struct cc_parser
{
    const cc_token* end;
    const cc_token* next;
    cc_heaprecord heap;
} cc_parser;

// TODO: Move everything to the arena?
// No more temporary stack pointers that way. (And thus, fewer defensive copies)

void cc_parser_create(cc_parser* parse, const cc_token* begin, const cc_token* end);
void cc_parser_destroy(cc_parser* parse);
int cc_parser_parse_type(cc_parser* parse, cc_ast_type* out_type);
int cc_parser_parse_decl(cc_parser* parse, cc_ast_decl* out_decl);
int cc_parser_parse_const(cc_parser* parse, cc_ast_const* out_const);
int cc_parser_parse_expr(cc_parser* parse, cc_ast_expr* out_expr);
int cc_parser_parse_stmt(cc_parser* parse, cc_ast_stmt* out_stmt);
int cc_parser_parse_body(cc_parser* parse, cc_ast_body* out_body);
static void* cc_parser_alloc(cc_parser* parse, size_t size) {
    return cc_heaprecord_alloc(&parse->heap, size);
}
static cc_parser_savestate cc_parser_save(const cc_parser* parse)
{
    cc_parser_savestate s = { parse->next, parse->heap.num_allocs };
    return s;
}
static void cc_parser_restore(cc_parser* parse, const cc_parser_savestate* save)
{
    parse->next = save->next;
    cc_heaprecord_pop(&parse->heap, parse->heap.num_allocs - save->num_allocs);
}