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
 * @param end (optional) End of the string. Use `nullptr` for a null-terminated string.
 */
void cc_lexer_init(cc_lexer* lex, const cc_char* begin, const cc_char* end);
/**
 * @brief Read a token and advance the lexer
 * @param out_tk Receives the next token
 * @return 0 if nothing was read
 */
int cc_lexer_read(cc_lexer* lex, cc_token* out_tk);
/**
 * @brief Create an array of all tokens that can be read.
 * Make sure to free the array after use.
 * @param begin A string with no null characters
 * @param end (optional) End of the string. Use `nullptr` for a null-terminated string.
 * @param out_array Receives the array pointer. Free after use.
 * @param out_len Receives the array length
 * @return 1 if all text was read. 0 if invalid text was encountered.
 */
int cc_lexer_readall(const cc_char* begin, const cc_char* end, cc_token** out_array, size_t* out_len);
/** The string length of a token */
static size_t cc_token_len(const cc_token* tk) { return (size_t)(tk->end - tk->begin) / sizeof(cc_char); }
/**
 * @brief The string length of a range of tokens
 * @param begin The first token
 * @param end A pointer past the end token
 */
static size_t cc_token_len_range(const cc_token* begin, const cc_token* end) {
    if (begin == end)
        return 0;
    return (size_t)((end-1)->end - begin->begin) / sizeof(cc_char);
}
/**
 * Compare a token's string with a null-terminated string
 * @param string A null-terminated string
 * @return 0 if the strings are identical
 */
int cc_token_strcmp(const cc_token* tk, const cc_char* string);