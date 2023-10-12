#pragma once

/**
 * @file
 * @brief Structs and defines for the C syntax tree
 */

typedef struct cc_token cc_token;

enum cc_ast_typeflag
{
    CC_AST_TYPEFLAG_CONST =    (1 << 0),
    CC_AST_TYPEFLAG_VOLATILE = (1 << 1),
    CC_AST_TYPEFLAG_SHORT =    (1 << 2),
    CC_AST_TYPEFLAG_LONG =     (1 << 3),
    CC_AST_TYPEFLAG_LONGLONG = (1 << 4),
    CC_AST_TYPEFLAG_SIGNED =   (1 << 5),
    CC_AST_TYPEFLAG_UNSIGNED = (1 << 6),
};

enum cc_ast_typeid
{
    CC_AST_TYPEID_INT,
    CC_AST_TYPEID_CHAR,
    CC_AST_TYPEID_VOID,
    CC_AST_TYPEID_POINTER,
    CC_AST_TYPEID_TYPEDEF,
    CC_AST_TYPEID_FUNCTION,
};

typedef struct cc_ast_type
{
    const cc_token* begin;
    const cc_token* end;
    
    /// @brief A value from @ref cc_ast_typeid
    int type_id;
    /// @brief A combination of values from @ref cc_ast_typeflag
    int type_flags;

    union
    {
        const struct cc_ast_type* pointer;
        // TODO: typedef namespaces: enum, union, struct, or global.
        const cc_token* type_def;
        struct
        {
            struct cc_ast_type* ret;
            struct cc_ast_decl_list* params;
        } func;
    } un;
    
} cc_ast_type;
 
typedef struct cc_ast_decl
{
    const cc_token* begin;
    const cc_token* end;

    cc_ast_type* type;
    const cc_token* name;
    /// @brief Non-zero if the storage duration is static
    int statik;
    /// @brief Optional, and for functions only. May be `NULL`.
    struct cc_ast_body* body;
} cc_ast_decl;

enum cc_ast_constid
{
    CC_AST_CONSTID_INT,
    CC_AST_CONSTID_FLOAT,
    CC_AST_CONSTID_STRING,
    CC_AST_CONSTID_CHAR,
};

typedef struct cc_ast_const
{
    const cc_token* token;
    int constid;
} cc_ast_const;

enum cc_ast_exprid
{
    // Atomic expressions
    CC_AST_EXPRID_CONST,
    CC_AST_EXPRID_VARIABLE,

    // Unary expressions
    CC_AST_EXPRID_REF,
    CC_AST_EXPRID_DEREF,
    CC_AST_EXPRID_CAST,
    CC_AST_EXPRID_INC, // Prefix increment
    CC_AST_EXPRID_DEC, // Prefix decrement
    CC_AST_SIZEOF,
    CC_AST_EXPRID_BOOL_NOT,
    CC_AST_EXPRID_BIT_NOT,

    // Binary expressions
    CC_AST_EXPRID_COMMA,
    CC_AST_EXPRID_ADD,
    CC_AST_EXPRID_SUB,
    CC_AST_EXPRID_MUL,
    CC_AST_EXPRID_DIV,
    CC_AST_EXPRID_MOD,
    CC_AST_EXPRID_LSHIFT,
    CC_AST_EXPRID_RSHIFT,
    CC_AST_EXPRID_MEMBER,
    CC_AST_EXPRID_MEMBER_DEREF,
    CC_AST_EXPRID_ASSIGN,
    CC_AST_EXPRID_BOOL_OR,
    CC_AST_EXPRID_BOOL_AND,
    CC_AST_EXPRID_BIT_OR,
    CC_AST_EXPRID_BIT_XOR,
    CC_AST_EXPRID_BIT_AND,
    CC_AST_EXPRID_COMPARE_LT, // Less than
    CC_AST_EXPRID_COMPARE_LTE, // Less than or equal
    CC_AST_EXPRID_COMPARE_GT, // Greater than
    CC_AST_EXPRID_COMPARE_GTE, // Greater than or equal
    CC_AST_EXPRID_COMPARE_EQ, // Equal to
    CC_AST_EXPRID_COMPARE_NEQ, // Not equal to

    // Ternary
    CC_AST_EXPRID_CONDITIONAL, // x ? y : z
};

typedef struct cc_ast_expr
{
    const cc_token* begin;
    const cc_token* end;
    /// @brief A value from @ref cc_ast_exprid
    int exprid;
    
    union
    {
        cc_ast_const konst;
        const cc_token* variable;
        struct {
            struct cc_ast_expr* lhs;
        } unary;
        struct {
            struct cc_ast_expr* lhs, * rhs;
        } binary;
        struct {
            struct cc_ast_expr* lhs, * middle, * rhs;
        } ternary;
    } un;
} cc_ast_expr;

typedef struct cc_ast_body
{
    const cc_token* begin;
    const cc_token* end;
    struct cc_ast_stmt* stmt;
} cc_ast_body;

typedef struct cc_ast_decl_list
{
    cc_ast_decl decl;
    struct cc_ast_decl_list* next;
} cc_ast_decl_list;

typedef struct cc_ast_for
{
    cc_ast_decl start;
    cc_ast_expr cond;
    cc_ast_expr end;
    cc_ast_body body;
} cc_ast_for;

enum cc_ast_stmtid
{
    CC_AST_STMTID_EXPR,
    CC_AST_STMTID_RETURN,
    CC_AST_STMTID_DECL,
    CC_AST_STMTID_IF,
    CC_AST_STMTID_WHILE,
    CC_AST_STMTID_DO_WHILE,
    CC_AST_STMTID_FOR,
    CC_AST_STMTID_GOTO,
    CC_AST_STMTID_LABEL,
    CC_AST_STMTID_BREAK,
    CC_AST_STMTID_CONTINUE,
};

typedef struct cc_ast_stmt
{
    const cc_token* begin;
    const cc_token* end;
    /// @brief The following statement, or `NULL`
    struct cc_ast_stmt* next;
    /// @brief A value from @ref cc_ast_stmtid
    int stmtid;

    union
    {
        cc_ast_expr expr;
        cc_ast_expr ret;
        cc_ast_decl decl;
        struct
        {
            cc_ast_expr cond;
            cc_ast_body body;
        } if_, while_, dowhile_;
        cc_ast_for for_;
        const cc_token* goto_;
        const cc_token* label;
    } un;
} cc_ast_stmt;

/// @return 0 if the type is invalid
int cc_ast_type_verify(const cc_ast_type* t);
/// @brief Validates the declaration, but not the type
/// @return 0 if the declaration is invalid
int cc_ast_decl_verify(const cc_ast_decl* d);