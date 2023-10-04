#include <cc/parser.h>
#include <cc/lib.h>

static const cc_token* cc_parser_peek(const cc_parser* parse)
{
    if (parse->next < parse->end)
        return parse->next;
    return NULL;
}

/// @brief Get the next token and advance if `tokenid` matches
/// @return nullptr if the next token doesn't match
static const cc_token* cc_parser_eat(cc_parser* parse, int tokenid)
{
    if (parse->next < parse->end && parse->next->tokenid == tokenid)
        return parse->next++;
    return NULL;
}

/// @param out_type Flags will be combined with existing ones, and a new `end` is stored
/// @return 0 if no flags were parsed 
static int cc_parser_parse_typeflags(cc_parser* parse, int* out_flags)
{
    const cc_token* begin = parse->next;
    const cc_token* tk;
    while (tk = cc_parser_peek(parse))
    {
        int flag = 0;
        switch (tk->tokenid)
        {
        case CC_TOKENID_CONST: flag = CC_AST_TYPEFLAG_CONST; break;
        case CC_TOKENID_SHORT: flag = CC_AST_TYPEFLAG_SHORT; break;
        case CC_TOKENID_LONG: flag = CC_AST_TYPEFLAG_LONG; break;
        case CC_TOKENID_SIGNED: flag = CC_AST_TYPEFLAG_SIGNED; break;
        case CC_TOKENID_UNSIGNED: flag = CC_AST_TYPEFLAG_UNSIGNED; break;
        case CC_TOKENID_VOLATILE: flag = CC_AST_TYPEFLAG_VOLATILE; break;
        }

        if (flag == 0)
            break;

        // Find double-flags
        switch (flag & *out_flags)
        {
        case CC_AST_TYPEFLAG_LONG:
            if (!(*out_flags & CC_AST_TYPEFLAG_LONGLONG))
            {
                *out_flags &= ~CC_AST_TYPEFLAG_LONG;
                *out_flags |= CC_AST_TYPEFLAG_LONGLONG;
            }
            break;
        default:
            *out_flags |= flag;
        }

        ++parse->next;
    }
    return parse->next != begin;
}

/// @brief Parses a single pointer. Non-recursive.
/// @param out_type Destination. May be the same pointer as `lhs`
/// @return 0 on failure
static int cc_parser_parse_typeptr(cc_parser* parse, const cc_ast_type* lhs, cc_ast_type* out_type)
{
    cc_parser_savestate save = cc_parser_save(parse);

    // Parse asterisk and flags
    if (!cc_parser_eat(parse, CC_TOKENID_ASTERISK))
        goto fail;

    // Copy lhs from stack to arena
    cc_ast_type* saved_lhs = (cc_ast_type*)cc_arena_alloc(&parse->arena, sizeof(*lhs));
    memcpy(saved_lhs, lhs, sizeof(*lhs));

    memset(out_type, 0, sizeof(*out_type));
    cc_parser_parse_typeflags(parse, &out_type->type_flags);
    out_type->type_id = CC_AST_TYPEID_POINTER;
    out_type->begin = saved_lhs->begin;
    out_type->un.pointer = saved_lhs;
    out_type->end = parse->next;

    // Verify type
    if (!cc_ast_type_verify(out_type))
    {
        if (lhs == out_type)
            memcpy(out_type, saved_lhs, sizeof(*lhs));
        goto fail;
    }
    
    return 1;

fail:
    cc_parser_restore(parse, &save);
    return 0;
}

void cc_parser_create(cc_parser* parse, const cc_token* begin, const cc_token* end)
{
    memset(parse, 0, sizeof(*parse));
    parse->end = end;
    parse->next = begin;
    parse->arena = cc_arena_create();
}

void cc_parser_destroy(cc_parser* parse) {
    cc_arena_destroy(parse->arena);
}

int cc_parser_parse_type(cc_parser* parse, cc_ast_type* out_type)
{    
    cc_parser_savestate save = cc_parser_save(parse);
    
    memset(out_type, 0, sizeof(*out_type));
    out_type->begin = parse->next;

    int got_flags = cc_parser_parse_typeflags(parse, &out_type->type_flags); // prefixed flags
    int got_type_id = 0;

    const cc_token* tk;
    if (tk = cc_parser_peek(parse))
    {
        int type_id = -1;
        switch (tk->tokenid)
        {
        case CC_TOKENID_INT:        type_id = CC_AST_TYPEID_INT; break;
        case CC_TOKENID_CHAR:       type_id = CC_AST_TYPEID_CHAR; break;
        case CC_TOKENID_VOID:       type_id = CC_AST_TYPEID_VOID; break;
        case CC_TOKENID_IDENTIFIER:
            type_id = CC_AST_TYPEID_TYPEDEF;
            out_type->un.type_def = tk;
            break;
        }
        if (type_id != -1)
        {
            got_type_id = 1;
            out_type->type_id = type_id;
            ++parse->next;
        }
    }
    
    if (!got_flags && !got_type_id)
        goto fail;
    
    if (got_type_id)
        cc_parser_parse_typeflags(parse, &out_type->type_flags); // postfixed flags
    else
        out_type->type_id = CC_AST_TYPEID_INT; // Assume int type
    
    out_type->end = parse->next;

    if (!cc_ast_type_verify(out_type))
        goto fail;
    
    while (cc_parser_parse_typeptr(parse, out_type, out_type))
        ;

    return 1;

fail:
    cc_parser_restore(parse, &save);
    return 0;
}

/// @brief Parse a function's parameter list, starting at the '(' token.
/// @param lhs A non-function type
/// @param out_type Destination. May be the same pointer as `lhs`
/// @return 0 on failure
int cc_parser_parse_functype(cc_parser* parse, const cc_ast_type* lhs, cc_ast_type* out_type)
{
    cc_parser_savestate save = cc_parser_save(parse);

    if (!cc_parser_eat(parse, CC_TOKENID_LEFT_ROUND))
        return 0;
    
    cc_ast_decl_list* params = NULL;
    cc_ast_decl_list** nextptr = &params;
    while (1)
    {
        // If it's our first loop, don't expect a comma
        if (nextptr != &params && !cc_parser_eat(parse, CC_TOKENID_COMMA))
            break;
        
        cc_parser_savestate interm_save = cc_parser_save(parse);
        cc_ast_decl_list* decl_item = (cc_ast_decl_list*)cc_arena_alloc(&parse->arena, sizeof(*decl_item));
        if (!cc_parser_parse_decl(parse, &decl_item->decl))
        {
            cc_parser_restore(parse, &interm_save);
            break;
        }
        decl_item->next = NULL;
        *nextptr = decl_item;
        nextptr = &decl_item->next;
    }

    if (!cc_parser_eat(parse, CC_TOKENID_RIGHT_ROUND))
        goto fail;
    
    // Nothing can possibly go wrong here!
    // Move lhs to the arena and start modifying out_type
    cc_ast_type* ret_type = (cc_ast_type*)cc_arena_alloc(&parse->arena, sizeof(*ret_type));
    memcpy(ret_type, lhs, sizeof(*ret_type));

    memset(out_type, 0, sizeof(*out_type));
    out_type->begin = ret_type->begin;
    out_type->end = parse->next;
    out_type->type_id = CC_AST_TYPEID_FUNCTION;
    out_type->un.func.ret = ret_type;
    out_type->un.func.params = params;
    return 1;

fail:
    cc_parser_restore(parse, &save);
    return 0;
}

int cc_parser_parse_decl(cc_parser* parse, cc_ast_decl* out_decl)
{
    cc_parser_savestate save = cc_parser_save(parse);
    
    memset(out_decl, 0, sizeof(*out_decl));
    out_decl->begin = parse->next;
    out_decl->statik = cc_parser_eat(parse, CC_TOKENID_STATIC) ? 1 : 0;

    out_decl->type = (cc_ast_type*)cc_arena_alloc(&parse->arena, sizeof(*out_decl->type));
    if (!cc_parser_parse_type(parse, out_decl->type)
        || !(out_decl->name = cc_parser_eat(parse, CC_TOKENID_IDENTIFIER)))
        goto fail;
    
    if (cc_parser_parse_functype(parse, out_decl->type, out_decl->type))
    {
        // Try to parse a body
        cc_parser_savestate interm_save = cc_parser_save(parse);
        out_decl->body = (cc_ast_body*)cc_arena_alloc(&parse->arena, sizeof(*out_decl->body));
        if (!cc_parser_parse_body(parse, out_decl->body))
        {
            out_decl->body = NULL;
            cc_parser_restore(parse, &interm_save);
        }
    }

    out_decl->end = parse->next;
    if (!cc_ast_decl_verify(out_decl))
        goto fail;
    return 1;

fail:
    cc_parser_restore(parse, &save);
    return 0;
}

int cc_parser_parse_const(cc_parser* parse, cc_ast_const* out_const)
{
    const cc_token* tk;
    if (tk = cc_parser_eat(parse, CC_TOKENID_INTCONST))
    {
        out_const->token = tk;
        out_const->constid = CC_AST_CONSTID_INT;
        return 1;
    }

    return 0;
}

int cc_parser_parse_expr_group(cc_parser* parse, cc_ast_expr* out_expr)
{
    cc_parser_savestate save = cc_parser_save(parse);

    const cc_token* begin;
    if (!(begin = cc_parser_eat(parse, CC_TOKENID_LEFT_ROUND)))
        return 0;
    
    if (!cc_parser_parse_expr(parse, out_expr)
        || !cc_parser_eat(parse, CC_TOKENID_RIGHT_ROUND))
        goto fail;
    
    out_expr->end = parse->next;
    return 1;

fail:
    cc_parser_restore(parse, &save);
    return 0;
}

int cc_parser_parse_expr_atomic(cc_parser* parse, cc_ast_expr* out_expr)
{
    if (cc_parser_parse_expr_group(parse, out_expr))
        return 1;
    
    out_expr->begin = parse->next;
    if (cc_parser_parse_const(parse, &out_expr->un.konst))
        out_expr->exprid = CC_AST_EXPRID_CONST;
    else if (out_expr->un.variable = cc_parser_eat(parse, CC_TOKENID_IDENTIFIER))
        out_expr->exprid = CC_AST_EXPRID_VARIABLE;
    else
        return 0;
    
    out_expr->end = parse->end;
    return 1;
}

int cc_parser_parse_expr_unary(cc_parser* parse, cc_ast_expr* out_expr)
{
    if (cc_parser_parse_expr_atomic(parse, out_expr))
    {
        // TODO: postfix
        return 1;
    }
    
    // Parse a prefixed unary operator
    cc_parser_savestate save = cc_parser_save(parse);

    out_expr->begin = parse->next;
    out_expr->exprid = -1;

    // Prefix operators
    if (cc_parser_eat(parse, CC_TOKENID_PLUSPLUS))
        out_expr->exprid = CC_AST_EXPRID_INC;
    else if (cc_parser_eat(parse, CC_TOKENID_MINUSMINUS))
        out_expr->exprid = CC_AST_EXPRID_DEC;
    else if (cc_parser_eat(parse, CC_TOKENID_AMP))
        out_expr->exprid = CC_AST_EXPRID_REF;
    else if (cc_parser_eat(parse, CC_TOKENID_ASTERISK))
        out_expr->exprid = CC_AST_EXPRID_DEREF;
    else
        return 0;
    
    cc_ast_expr* lhs = (cc_ast_expr*)cc_arena_alloc(&parse->arena, sizeof(*lhs));
    if (!cc_parser_parse_expr_atomic(parse, lhs));
        goto fail;
    
    out_expr->un.unary.lhs = lhs;
    out_expr->end = parse->end;
    return 1;

fail:
    cc_parser_restore(parse, &save);
    return 0;
}

// FIXME: This shouldn't just grab a unary for the rhs. It must include lower-precedence binaries
/// @brief Parses a single binary expression. Non-recursive.
/// @param out_expr Destination. May be the same pointer as `lhs`
/// @return 0 on failure
int cc_parser_parse_binary(cc_parser* parse, const cc_ast_expr* lhs, cc_ast_expr* out_expr)
{   
    cc_parser_savestate save = cc_parser_save(parse);
    
    int exprid;
    const cc_token* tk;
    if (tk = cc_parser_peek(parse))
    {
        switch (tk->tokenid)
        {
        case CC_TOKENID_PLUS:           exprid = CC_AST_EXPRID_ADD; break;
        case CC_TOKENID_MINUS:          exprid = CC_AST_EXPRID_SUB; break;
        case CC_TOKENID_ASTERISK:       exprid = CC_AST_EXPRID_MUL; break;
        case CC_TOKENID_SLASH:          exprid = CC_AST_EXPRID_DIV; break;
        case CC_TOKENID_PERCENT:        exprid = CC_AST_EXPRID_MOD; break;
        case CC_TOKENID_CARET:          exprid = CC_AST_EXPRID_BIT_OR; break;
        case CC_TOKENID_AMP:            exprid = CC_AST_EXPRID_BIT_AND; break;
        case CC_TOKENID_PIPE:           exprid = CC_AST_EXPRID_BIT_OR; break;
        case CC_TOKENID_TILDE:          exprid = CC_AST_EXPRID_BIT_NOT; break;
        case CC_TOKENID_AMPAMP:         exprid = CC_AST_EXPRID_BOOL_AND; break;
        case CC_TOKENID_PIPEPIPE:       exprid = CC_AST_EXPRID_BOOL_OR; break;
        case CC_TOKENID_EXCLAMATION:    exprid = CC_AST_EXPRID_BOOL_NOT; break;
        case CC_TOKENID_EQUAL:          exprid = CC_AST_EXPRID_ASSIGN; break;
        case CC_TOKENID_LEFT_ANGLE:     exprid = CC_AST_EXPRID_COMPARE_LT; break;
        case CC_TOKENID_RIGHT_ANGLE:    exprid = CC_AST_EXPRID_COMPARE_GT; break;
        default:
            return 0; // Parser state was unchanged, so simply return 0
        }
        ++parse->next;
    }

    cc_ast_expr* rhs = (cc_ast_expr*)cc_arena_alloc(&parse->arena, sizeof(*rhs));
    if (!cc_parser_parse_expr_unary(parse, rhs))
        goto fail;
    
    // Move lhs to arena
    cc_ast_expr* safe_lhs = (cc_ast_expr*)cc_arena_alloc(&parse->arena, sizeof(*lhs));
    memcpy(safe_lhs, lhs, sizeof(*lhs));

    out_expr->begin = lhs->begin;
    out_expr->un.binary.lhs = safe_lhs;
    out_expr->un.binary.rhs = rhs;
    out_expr->exprid = exprid;
    out_expr->end = rhs->end;
    return 1;

fail:
    cc_parser_restore(parse, &save);
    return 0;
}

int cc_parser_parse_expr(cc_parser* parse, cc_ast_expr* out_expr)
{
    if (!cc_parser_parse_expr_unary(parse, out_expr))
        return 0;
    
    while (cc_parser_parse_binary(parse, out_expr, out_expr))
        ;

    return 1;
}

int cc_parser_parse_stmt_label(cc_parser* parse, cc_ast_stmt* out_stmt)
{
    cc_parser_savestate save = cc_parser_save(parse);
    
    out_stmt->begin = parse->next;
    if (!(out_stmt->un.label = cc_parser_eat(parse, CC_TOKENID_IDENTIFIER)))
        return 0;
    if (!cc_parser_eat(parse, CC_TOKENID_COLON))
        goto fail;
    
    out_stmt->stmtid = CC_AST_STMTID_LABEL;
    out_stmt->end = parse->next;
    out_stmt->next = NULL;
    return 1;

fail:
    cc_parser_restore(parse, &save);
    return 0;
}

int cc_parser_parse_stmt_if(cc_parser* parse, cc_ast_stmt* out_stmt)
{
    cc_parser_savestate save = cc_parser_save(parse);
    
    out_stmt->begin = parse->next;
    if (!cc_parser_eat(parse, CC_TOKENID_IF))
        return 0;

    if (!cc_parser_eat(parse, CC_TOKENID_LEFT_ROUND)
        || !cc_parser_parse_expr(parse, &out_stmt->un.if_.cond)
        || !cc_parser_eat(parse, CC_TOKENID_RIGHT_ROUND))
        goto fail;
    
    if (!cc_parser_parse_body(parse, &out_stmt->un.if_.body))
        goto fail;
    
    out_stmt->end = parse->next;
    out_stmt->stmtid = CC_AST_STMTID_IF;
    out_stmt->next = NULL;
    return 1;
    
fail:
    cc_parser_restore(parse, &save);
    return 0;
}

int cc_parser_parse_stmt(cc_parser* parse, cc_ast_stmt* out_stmt)
{
    // Statements that do not end at a semicolon:
    if (cc_parser_parse_stmt_label(parse, out_stmt)
        || cc_parser_parse_stmt_if(parse, out_stmt))
        return 1;
    
    // Statements that end at a semicolon:
    cc_parser_savestate save = cc_parser_save(parse);

    out_stmt->begin = parse->next;
    if (cc_parser_parse_expr(parse, &out_stmt->un.expr))
        out_stmt->stmtid = CC_AST_STMTID_EXPR;
    else if (cc_parser_parse_decl(parse, &out_stmt->un.decl))
        out_stmt->stmtid = CC_AST_STMTID_DECL;
    else if (cc_parser_eat(parse, CC_TOKENID_RETURN))
    {
        if (!cc_parser_parse_expr(parse, &out_stmt->un.ret))
            goto fail;
        out_stmt->stmtid = CC_AST_STMTID_RETURN;
    }
    else if (cc_parser_eat(parse, CC_TOKENID_GOTO))
    {
        if (!(out_stmt->un.goto_ = cc_parser_eat(parse, CC_TOKENID_IDENTIFIER)))
            goto fail;
        out_stmt->stmtid = CC_AST_STMTID_GOTO;
    }
    else if (cc_parser_eat(parse, CC_TOKENID_CONTINUE))
        out_stmt->stmtid = CC_AST_STMTID_CONTINUE;
    else if (cc_parser_eat(parse, CC_TOKENID_BREAK))
        out_stmt->stmtid = CC_AST_STMTID_BREAK;
    else
        goto fail;
    
    if (!cc_parser_eat(parse, CC_TOKENID_SEMICOLON))
        goto fail;

    out_stmt->end = parse->next;
    out_stmt->next = NULL;
    return 1;

fail:
    cc_parser_restore(parse, &save);
    return 0;
}

int cc_parser_parse_body(cc_parser* parse, cc_ast_body* out_body)
{
    cc_parser_savestate save = cc_parser_save(parse);

    memset(out_body, 0, sizeof(*out_body));
    out_body->begin = parse->next;
    out_body->stmt = NULL;

    if (!cc_parser_eat(parse, CC_TOKENID_LEFT_CURLY))
        return 0;

    cc_ast_stmt** nextptr = &out_body->stmt;
    while (1)
    {
        cc_parser_savestate interm_save = cc_parser_save(parse);
        cc_ast_stmt* next = (cc_ast_stmt*)cc_arena_alloc(&parse->arena, sizeof(*next));
        if (!cc_parser_parse_stmt(parse, next))
        {
            cc_parser_restore(parse, &interm_save);
            break;
        }
        
        *nextptr = next;
        nextptr = &next->next;
    }
    
    if (!cc_parser_eat(parse, CC_TOKENID_RIGHT_CURLY))
        goto fail;
    out_body->end = parse->next;
    return 1;

fail:
    cc_parser_restore(parse, &save);
    return 0;
}