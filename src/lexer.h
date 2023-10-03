#pragma once
#include "lib.h"

enum cc_tokenid
{
    // Keywords, constants
    CC_TOKENID_IDENTIFIER,
    CC_TOKENID_INTCONST,
    CC_TOKENID_INT,
    CC_TOKENID_CHAR,
    CC_TOKENID_VOID,
    CC_TOKENID_CONST,
    CC_TOKENID_SHORT,
    CC_TOKENID_LONG,
    CC_TOKENID_SIGNED,
    CC_TOKENID_UNSIGNED,
    CC_TOKENID_VOLATILE,
    CC_TOKENID_STATIC,
    CC_TOKENID_IF,
    CC_TOKENID_ELSE,
    CC_TOKENID_WHILE,
    CC_TOKENID_GOTO,
    CC_TOKENID_RETURN,
    CC_TOKENID_BREAK,
    CC_TOKENID_CONTINUE,

    // Operators, punctuation
    CC_TOKENID_PLUSPLUS,
    CC_TOKENID_PLUS,
    CC_TOKENID_MINUSMINUS,
    CC_TOKENID_MINUS,
    CC_TOKENID_SLASH,
    CC_TOKENID_PERCENT,
    CC_TOKENID_ASTERISK,
    CC_TOKENID_AMPAMP,
    CC_TOKENID_AMP,
    CC_TOKENID_PIPEPIPE,
    CC_TOKENID_PIPE,
    CC_TOKENID_CARET,
    CC_TOKENID_EQUALEQUAL,
    CC_TOKENID_EQUAL,
    CC_TOKENID_COMMA,
    CC_TOKENID_DOT,
    CC_TOKENID_COLON,
    CC_TOKENID_SEMICOLON,
    CC_TOKENID_EXCLAMATION,
    CC_TOKENID_ARROW,
    CC_TOKENID_TILDE,
    CC_TOKENID_LEFT_CURLY,
    CC_TOKENID_RIGHT_CURLY,
    CC_TOKENID_LEFT_ROUND,
    CC_TOKENID_RIGHT_ROUND,
    CC_TOKENID_LEFT_SQUARE,
    CC_TOKENID_RIGHT_SQUARE,
    CC_TOKENID_LEFT_ANGLE,
    CC_TOKENID_RIGHT_ANGLE,
};

typedef struct cc_lexer
{
    const cc_char* str;
    const cc_char* end;
} cc_lexer;

typedef struct cc_token
{
    const cc_char* begin;
    const cc_char* end;
    int tokenid;
} cc_token;

/**
 * @brief Initialize a lexer with the string to be read
 * @param begin A string with no null characters
 * @param end (optional) End of the string. `nullptr` works for a null-terminated string.
 */
void cc_lexer_init(cc_lexer* lex, const cc_char* begin, const cc_char* end);
/**
 * @brief Read a token and advance the lexer
 * @param out_tk Location to store the read token
 * @return 0 if nothing was read
 */
int cc_lexer_read(cc_lexer* lex, cc_token* out_tk);
/** Length of the token's string, in bytes */
static size_t cc_token_len(const cc_token* tk) { return tk->end - tk->begin; }
/**
 * Compare a token's string with a null-terminated string
 * @param string A null-terminated string
 * @return 0 if the strings are identical
 */
static int cc_token_strcmp(const cc_token* tk, const cc_char* string)
{
    size_t tk_len = cc_token_len(tk);
    if (tk_len == 0 && string[0] == 0)
        return 0;

    for (size_t i = 0; i < tk_len; ++i)
    {
        int cmp = tk->begin[i] - string[i];
        if (cmp) // Assuming tk has no null chars, this will return when string ends
            return cmp;
    }
    return 0;
}
