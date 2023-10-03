#include "lexer.h"

typedef struct cc_keyword
{
    const cc_char* str;
    int tokenid;
} cc_keyword;

static cc_keyword cc_keyword_list[] =
{
    {"int", CC_TOKENID_INT},
    {"char", CC_TOKENID_CHAR},
    {"void", CC_TOKENID_VOID},
    {"const", CC_TOKENID_CONST},
    {"short", CC_TOKENID_SHORT},
    {"long", CC_TOKENID_LONG},
    {"signed", CC_TOKENID_SIGNED},
    {"unsigned", CC_TOKENID_UNSIGNED},
    {"volatile", CC_TOKENID_VOLATILE},
    {"static", CC_TOKENID_STATIC},
    {"if", CC_TOKENID_IF},
    {"else", CC_TOKENID_ELSE},
    {"while", CC_TOKENID_WHILE},
    {"goto", CC_TOKENID_GOTO},
    {"return", CC_TOKENID_RETURN},
    {"break", CC_TOKENID_BREAK},
    {"continue", CC_TOKENID_CONTINUE},
    {NULL, 0} // Null terminator. Do not remove.
};

// Order all overlapping tokens (like "++" and "+") with the longer one first
static cc_keyword cc_punctuation_list[] =
{
    {"++", CC_TOKENID_PLUSPLUS},
    {"+", CC_TOKENID_PLUS},
    {"-", CC_TOKENID_MINUS},
    {"--", CC_TOKENID_MINUSMINUS},
    {"/", CC_TOKENID_SLASH},
    {"%", CC_TOKENID_PERCENT},
    {"*", CC_TOKENID_ASTERISK},
    {"&&", CC_TOKENID_AMPAMP},
    {"&", CC_TOKENID_AMP},
    {"||", CC_TOKENID_PIPEPIPE},
    {"|", CC_TOKENID_PIPE},
    {"^", CC_TOKENID_CARET},
    {"==", CC_TOKENID_EQUALEQUAL},
    {"=", CC_TOKENID_EQUAL},
    {",", CC_TOKENID_COMMA},
    {".", CC_TOKENID_DOT},
    {":", CC_TOKENID_COLON},
    {";", CC_TOKENID_SEMICOLON},
    {"!", CC_TOKENID_EXCLAMATION},
    {"->", CC_TOKENID_ARROW},
    {"~", CC_TOKENID_TILDE},
    {"{", CC_TOKENID_LEFT_CURLY},
    {"}", CC_TOKENID_RIGHT_CURLY},
    {"(", CC_TOKENID_LEFT_ROUND},
    {")", CC_TOKENID_RIGHT_ROUND},
    {"[", CC_TOKENID_LEFT_SQUARE},
    {"]", CC_TOKENID_RIGHT_SQUARE},
    {"<", CC_TOKENID_LEFT_ANGLE},
    {">", CC_TOKENID_RIGHT_ANGLE},
    {NULL, 0} // Null terminator. Do not remove.
};

static int cc_lexer_peek(const cc_lexer* lex, cc_char* out_char)
{
    if (lex->str >= lex->end)
        return 0;
    *out_char = *lex->str;
    return 1;
}

static int cc_lexer_match(cc_lexer* lex, cc_char value)
{
    if (lex->str >= lex->end || *lex->str != value)
        return 0;
    ++lex->str;
    return 1;
}

static void cc_lexer_skipwhitespace(cc_lexer* lex)
{
    while (lex->str < lex->end)
    {
        if (!cc_isspace(*lex->str))
            break;
        ++lex->str;
    }
}

static int cc_lexer_read_identifier(const cc_lexer* lex, cc_token* out_tk)
{
    const cc_char* next = lex->str;
    if (next < lex->end && cc_isdigit(*next))
        return 0;
    
    while (next < lex->end && (cc_isalnum(*next) || *next == '_'))
        ++next;
    
    if (next == lex->str) // If we haven't moved
        return 0;
    
    out_tk->begin = lex->str;
    out_tk->end = next;
    out_tk->tokenid = CC_TOKENID_IDENTIFIER;
    return 1;
}

static int cc_lexer_read_intconst(const cc_lexer* lex, cc_token* out_tk)
{
    const cc_char* next = lex->str;
    while (next < lex->end && cc_isdigit(*next))
        ++next;
    if (next == lex->str)
        return 0;
    out_tk->begin = lex->str;
    out_tk->end = next;
    out_tk->tokenid = CC_TOKENID_INTCONST;
    return 1;
}

static int cc_lexer_read_punctuation(const cc_lexer* lex, cc_token* out_tk)
{
    size_t remaining = lex->end - lex->str;
    if (remaining == 0)
        return 0;
    
    for (const cc_keyword* kw = &cc_punctuation_list[0]; kw->str; ++kw)
    {
        size_t kw_len = cc_strlen(kw->str);
        if (kw_len <= remaining && !cc_strncmp(lex->str, kw->str, kw_len))
        {
            out_tk->begin = lex->str;
            out_tk->end = lex->str + kw_len;
            out_tk->tokenid = kw->tokenid;
            return 1;
        }
    }

    return 0;
}

void cc_lexer_init(cc_lexer* lex, const cc_char* begin, const cc_char* end)
{
    lex->str = begin;
    if (!end)
        end = begin + cc_strlen(begin);
    lex->end = end;
}

int cc_lexer_read(cc_lexer* lex, cc_token* out_tk)
{
    cc_lexer_skipwhitespace(lex);
    
    int found = 1;
    if (cc_lexer_read_identifier(lex, out_tk))
    {
        // An identifier may be a keyword. Do a lookup to find out.
        for (const cc_keyword* kw = &cc_keyword_list[0]; kw->str; ++kw)
        {
            if (cc_strlen(kw->str) == cc_token_len(out_tk)
                && !cc_strncmp(out_tk->begin, kw->str, cc_token_len(out_tk)))
            {
                out_tk->tokenid = kw->tokenid;
                break;
            }
        }
    } else if (cc_lexer_read_intconst(lex, out_tk)) {
    } else if (cc_lexer_read_punctuation(lex, out_tk)) {
    } else {
        found = 0;
    }

    if (found)
        lex->str = out_tk->end;

    return found;
}