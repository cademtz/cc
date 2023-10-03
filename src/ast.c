#include "ast.h"

int cc_ast_type_verify(const cc_ast_type* t)
{
    int size_specifiers = 0;
    if (t->type_flags & CC_AST_TYPEFLAG_SHORT) ++size_specifiers;
    if (t->type_flags & CC_AST_TYPEFLAG_LONG) ++size_specifiers;
    if (t->type_flags & CC_AST_TYPEFLAG_LONGLONG) ++size_specifiers;

    int sign_specifiers = 0;
    if (t->type_flags & CC_AST_TYPEFLAG_SIGNED) ++sign_specifiers;
    if (t->type_flags & CC_AST_TYPEFLAG_UNSIGNED) ++sign_specifiers;

    int is_integral = t->type_id == CC_AST_TYPEID_INT || t->type_id == CC_AST_TYPEID_CHAR;

    if (!is_integral && (size_specifiers || sign_specifiers))
        return 0; // Only integer types can have sign and size specifiers
    if (size_specifiers > 1)
        return 0; // Conflicting sizes
    return 1;
}

int cc_ast_decl_verify(const cc_ast_decl* d)
{
    if (d->type->type_id == CC_AST_TYPEID_VOID)
        return 0; // Cannot declare a void
    return 1;
}