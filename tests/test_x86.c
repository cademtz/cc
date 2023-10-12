#include "test.h"
#include <cc/x86_gen.h>

#define equal_code(func, block_index, expected) equal__code(func, block_index, expected, sizeof(expected) - 1)

/// @brief Check if an x86 block starts with the full sequence of bytes in `expected`
/// @param block_index The block index, or `(size_t)-1` for the last block
/// @param expected The expected x86 code
/// @param expected_len Length of expected code
/// @return `true` if the expected code matches
static int equal__code(const x86func* func, size_t block_index, const char* expected, size_t expected_len)
{
    if (block_index == (size_t)-1)
        block_index = func->num_blocks - 1;
    
    uint8_t* code = func->code + func->blocks[block_index];
    uint8_t* end = func->code + func->size_code;
    if (block_index + 1 < func->num_blocks)
        end = func->code + func->blocks[block_index + 1];

    size_t min_len = end - code;
    if (min_len < expected_len)
        return 0;
    return memcmp(code, expected, min_len) == 0;
}

int test_x86(void)
{
    x86func func;

    { // Test return
        x86func_create(&func);
        x86func_ret(&func);
        test_assert("Expected opcode 0xC3", equal_code(&func, -1, "\xC3"));
        x86func_destroy(&func);
    }
    { // Test various operands

    }
    
    return 1;
}