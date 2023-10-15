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

void x86func_add(x86func* func, x86_regmem dst, x86_regmem src)
{
    // dst may not be a constant
    if (dst.type == X86_REGMEM_CONST)
        return;

    bool dst_direct = dst.type == X86_REGMEM_REG || dst.type == X86_REGMEM_CONST;
    bool src_direct = src.type == X86_REGMEM_REG || src.type == X86_REGMEM_CONST;

    // There may be one indirect operand at most. It cannot be both.
    if (!dst_direct && !src_direct)
        return;
    
    // If we're working with an indirect operand, gather some initial info
    bool use_sib = false; // If true, an SIB is emitted
    uint8_t imm_size = 0; // Size of immediate to emit, in bytes
    uint32_t imm = 0;  // The immediate to emit
    uint8_t rex = 0;   // The REX to emit, if non-zero
    uint8_t modrm = 0; // The ModRM to emit
    uint8_t sib = 0;   // The SIB to emit
    const x86_regmem* indirect = NULL;
    const x86_regmem* direct = NULL;

    if (!dst_direct)
        indirect = &dst, direct = &src;
    else if (!src_direct)
        indirect = &src, direct = &dst;
    
    if (indirect) // An indirect register or offset
    {
        if (direct->reg >= X86_REG_R8 && direct->reg <= X86_REG_R15)
            rex |= X86_REX_R; // Extend the direct register
        
        if (indirect->type == X86_REGMEM_MEM) // An indirect register, such as [eax] or [eax+ecx*16+4096]
        {
            if (indirect->index >= X86_REG_R8 && indirect->index <= X86_REG_R15)
                rex |= X86_REX_X; // Extend SIB.index
            if (indirect->reg >= X86_REG_R8 && indirect->reg <= X86_REG_R15)
                rex |= X86_REX_B; // Extend the base register

            // Things that require the SIB byte:
            // - Indirection on SP (SP is used to indicate an SIB byte)
            // - Using a register as an index
            use_sib = indirect->reg == X86_REG_SP || indirect->index != X86_REG_SP;
            if (use_sib)
            {
                modrm = x86_modrm(X86_MOD_DISP0, 0, X86_REG_SP);
                sib = x86_sib(indirect->scale, indirect->index, indirect->reg);
            }
            else 
                modrm = x86_modrm(X86_MOD_DISP0, 0, indirect->reg);
    
            // Things that require a displacement:
            // - A displacement
            // - Indirection on BP (BP with disp0 indicates [disp32], so we must use [bp+disp8] instead)
            if (indirect->offset || indirect->reg == X86_REG_BP) // Emit the offset
            {
                if (indirect->offset < INT8_MIN || indirect->offset > INT8_MAX)
                    imm_size = 4, modrm |= x86_modrm(X86_MOD_DISP32, 0, 0);
                else
                    imm_size = 1, modrm |= x86_modrm(X86_MOD_DISP8, 0, 0);
                imm = (uint32_t)indirect->offset;
            }
        }
        else // An indirect offset, such as [ds:0x100] or [RIP+0x100]
        {
            // In protected mode, this will emit ds:offset32
            // In long mode, this will emit [RIP+offset32]
            modrm = x86_modrm(X86_MOD_DISP0, 0, X86_REG_BP);
        }
    }
    else // No indirection
    {
        // Assume dst in r/m
        modrm = x86_modrm(X86_MOD_DIRECT, 0, dst.reg);
        // dst will be the reg/mem operand
        if (dst.reg >= X86_REG_R8 && dst.reg <= X86_REG_R15)
            rex |= X86_REX_B;
        if (src.reg >= X86_REG_R8 && src.reg <= X86_REG_R15)
            rex |= X86_REX_R;
    }
    
    if (rex)
        x86func_byte(func, X86_REX__REX | rex);
    
    // Add two reg/mem operands
    if ((src.type == X86_REGMEM_REG || src.type == X86_REGMEM_MEM)
        && (dst.type == X86_REGMEM_REG || dst.type == X86_REGMEM_MEM))
    {
        // Side effect: With no indirection, dst is placed in r/m by default.
        // Always use src_direct first.
        if (src_direct)
        {
            x86func_byte(func, 1); // ADD reg/mem64, reg64      01 /r
            x86func_byte(func, modrm | x86_modrm(0, src.reg, 0));
        }
        else
        {
            x86func_byte(func, 3); // ADD reg64, reg/mem64      03 /r
            x86func_byte(func, modrm | x86_modrm(0, dst.reg, 0));
        }
    }
    else // Add a const to a reg/mem operand
    {

    }

    if (use_sib)
        x86func_byte(func, sib);
    
    if (imm_size == 1)
        x86func_byte(func, (uint8_t)imm);
    else if (imm_size == 4)
        x86func_imm32(func, imm);
}

void x86func_ret(x86func* func) {
    x86func_byte(func, 0xC3);
}