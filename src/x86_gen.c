#include <cc/x86_gen.h>

/// @brief Append `num_bytes` bytes to `func->code`
/// @return Pointer to the appended bytes (do not save it)
static uint8_t* x86func_append(x86func* func, size_t num_bytes)
{
    size_t pos = func->size_code;
    func->size_code += num_bytes;
    func->code = (uint8_t*)realloc(func->code, func->size_code);
    return func->code + pos;
}

void x86func_create(x86func* func)
{
    memset(func, 0, sizeof(*func));
    func->blocks = (size_t*)malloc(sizeof(func->blocks[0]));
    func->blocks[0] = 0;
    func->num_blocks = 1;
}
void x86func_destroy(x86func* func)
{
    free(func->code);
    free(func->blocks);
}

size_t x86func_block(x86func* func)
{
    if (func->size_code == 0)
        return 0;
    ++func->num_blocks;
    func->blocks = (size_t*)realloc(func->blocks, func->num_blocks * sizeof(func->blocks[0]));
    func->blocks[func->num_blocks - 1] = func->size_code;
    return func->num_blocks - 1;
}

void x86func_byte(x86func* func, uint8_t byte) {
    *x86func_append(func, 1) = byte;
}
void x86func_imm16(x86func* func, uint16_t imm)
{
    uint8_t* dst = x86func_append(func, sizeof(imm));
    for (int i = 0; i < sizeof(imm); ++i)
        dst[i] = (imm >> (i * 8)) & 0xFF;
}
void x86func_imm32(x86func* func, uint32_t imm)
{
    uint8_t* dst = x86func_append(func, sizeof(imm));
    for (int i = 0; i < sizeof(imm); ++i)
        dst[i] = (imm >> (i * 8)) & 0xFF;
}
void x86func_imm64(x86func* func, uint64_t imm)
{
    uint8_t* dst = x86func_append(func, sizeof(imm));
    for (int i = 0; i < sizeof(imm); ++i)
        dst[i] = (imm >> (i * 8)) & 0xFF;
}

void x86func_ret(x86func* func) {
    x86func_byte(func, 0xC3);
}