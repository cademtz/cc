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

/**
 * @brief Emit the encoded reg/mem and reg operands.
 * 
 * Only one memory operand is allowed.
 * The operands may only be a register or a memory operand.
 * When both operands are direct registers, `lhs` is assumed to be the `rm` field of `ModRM.rm`.
 * 
 * For certain instructions, the `ModRM.reg` field is part of the opcode.
 * In that case, set either `lhs` or `rhs` to `x86_reg(...)`.
 * @return True if the operands are valid and appended
 */
static bool x86func_regmem(x86func* func, x86_regmem lhs, x86_regmem rhs)
{
    bool lhs_direct = lhs.type == X86_REGMEM_REG;
    bool rhs_direct = rhs.type == X86_REGMEM_REG;

    // There may be one indirect operand at most. It cannot be both.
    if (!lhs_direct && !rhs_direct)
        return false;
    
    // If we're working with an indirect operand, gather some initial info
    bool use_sib = false;   // If true, an SIB is emitted
    uint8_t imm_size = 0;   // Size of immediate to emit, in bytes
    uint32_t imm = 0;       // The immediate to emit
    uint8_t modrm = 0;      // The ModRM to emit
    uint8_t sib = 0;        // The SIB to emit
    const x86_regmem* indirect = NULL;
    const x86_regmem* direct = NULL;

    if (!lhs_direct)
        indirect = &lhs, direct = &rhs;
    else if (!rhs_direct)
        indirect = &rhs, direct = &lhs;

    if (indirect) // An indirect register or offset
    {
        if (indirect->type == X86_REGMEM_MEM) // An indirect register, such as [eax] or [eax+ecx*16+4096]
        {
            // Things that require the SIB byte:
            // - Indirection on SP (SP is used to indicate an SIB byte)
            // - Using a register as an index
            use_sib = indirect->reg == X86_REG_SP || indirect->index != X86_REG_SP;
            if (use_sib)
            {
                modrm = x86_modrm(X86_MOD_DISP0, direct->reg, X86_REG_SP);
                sib = x86_sib(indirect->scale, indirect->index, indirect->reg);
            }
            else 
                modrm = x86_modrm(X86_MOD_DISP0, direct->reg, indirect->reg);
    
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
            imm_size = 4;
            imm = (uint32_t)indirect->offset;
        }
    }
    else // No indirection
    {
        // Assume lhs in r/m
        modrm = x86_modrm(X86_MOD_DIRECT, rhs.reg, lhs.reg);
    }
    
    x86func_imm8(func, modrm);
    if (use_sib)
        x86func_imm8(func, sib);
    if (imm_size == 1)
        x86func_imm8(func, (uint8_t)imm);
    else if (imm_size == 4)
        x86func_imm32(func, imm);
    return true;
}

/**
 * @brief Emit various operand-affecting  prefixes (REX and others) where necessary.
 * 
 * Only one memory operand is allowed.
 * When both operands are direct registers, `lhs` is assumed to be the `rm` field of `ModRM.rm`.
 * @param opsize A value from @ref x86_opsize
 * @param lhs Any operand
 * @param rhs Any operand.
 * If the instruction has no other operand, a placeholder register from 0 to 7 will work (`x86_reg(0)`)
 * @return True if the operands are valid
 */
static bool x86func_rex_binary(x86func* func, uint8_t opsize, x86_regmem lhs, x86_regmem rhs)
{
    const x86_regmem* indirect = NULL; // An indirect register operand
    const x86_regmem* direct = NULL;
    uint8_t rex = 0; // The lower REX nibble. Emitted if non-zero.

    if (lhs.type == X86_REGMEM_MEM)
        indirect = &lhs, direct = &rhs;
    else if (rhs.type == X86_REGMEM_MEM)
        indirect = &rhs, direct = &lhs;
    
    if (indirect)
    {
        if (direct->type == X86_REGMEM_MEM || direct->type == X86_REGMEM_OFFSET)
            return false; // `direct` is a memory operand despite one already existing. That will not work.

        if (direct->reg >= X86_REG_R8 && direct->reg <= X86_REG_R15)
            rex |= X86_REX_R; // Extend the direct register
        
        if (indirect->index >= X86_REG_R8 && indirect->index <= X86_REG_R15)
            rex |= X86_REX_X; // Extend SIB.index
        if (indirect->reg >= X86_REG_R8 && indirect->reg <= X86_REG_R15)
            rex |= X86_REX_B; // Extend the base register
    }
    else
    {
        if (lhs.reg >= X86_REG_R8 && lhs.reg <= X86_REG_R15)
            rex |= X86_REX_B;
        if (rhs.reg >= X86_REG_R8 && rhs.reg <= X86_REG_R15)
            rex |= X86_REX_R;
    }

    switch (func->mode)
    {
    case X86_MODE_REAL:
        if (opsize > X86_OPSIZE_WORD)
            return false;
        break;
    case X86_MODE_PROTECTED:
        if (opsize > X86_OPSIZE_DWORD)
            return false;
        if (opsize == X86_OPSIZE_WORD)
            x86func_imm8(func, 0x66);
        break;
    case X86_MODE_LONG:
        if (opsize == X86_OPSIZE_WORD)
            x86func_imm8(func, 0x66);
        else if (opsize == X86_OPSIZE_QWORD)
            rex |= X86_REX_W;
    break;
    default:
        assert(0 && "This execution mode was not implemented");
    }

    if (rex)
        x86func_imm8(func, rex | X86_REX__REX);
    return true;
}

void x86func_create(x86func* func, uint8_t mode)
{
    memset(func, 0, sizeof(*func));
    func->mode = mode;
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

void x86func_imm8(x86func* func, uint8_t byte) {
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

void x86func_add(x86func* func, uint8_t opsize, x86_regmem dst, x86_regmem src)
{
    // dst may not be a constant
    if (dst.type == X86_REGMEM_CONST)
        return;
    
    if (!x86func_rex_binary(func, opsize, dst, src))
        return;
    
    if (src.type == X86_REGMEM_CONST) // Add a constant value
    {
        if (opsize == X86_OPSIZE_BYTE)
        {
            x86func_imm8(func, 0x80);
            x86func_regmem(func, dst, x86_reg(0));
            x86func_imm8(func, (uint8_t)src.offset);
        }
        else if (src.offset < INT8_MIN || src.offset > INT8_MAX)
        {
            x86func_imm8(func, 0x81);
            x86func_regmem(func, dst, x86_reg(0));
            if (func->mode == X86_MODE_REAL || opsize == X86_OPSIZE_WORD)
                x86func_imm16(func, (uint16_t)src.offset);
            else
                x86func_imm32(func, (uint32_t)src.offset);
        }
        else
        {
            x86func_imm8(func, 0x83);
            x86func_regmem(func, dst, x86_reg(0));
            x86func_imm8(func, (uint8_t)src.offset);
        }
    }
    else if (src.type == X86_REGMEM_REG) // Add a register
    {
        x86func_imm8(func, opsize == X86_OPSIZE_BYTE ? 0 : 1); // ADD reg/mem64, reg64      01 /r
        x86func_regmem(func, dst, src);
    }
    else // Add a value in memory
    {
        x86func_imm8(func, opsize == X86_OPSIZE_BYTE ? 2 : 3); // ADD reg64, reg/mem64      03 /r
        x86func_regmem(func, dst, src);
    }
}

void x86func_sub(x86func* func, uint8_t opsize, x86_regmem dst, x86_regmem src)
{
    if (dst.type == X86_REGMEM_CONST)
        return;
    
    if (!x86func_rex_binary(func, opsize, dst, src))
        return;
    
    if (src.type == X86_REGMEM_CONST)
    {
        if (opsize == X86_OPSIZE_BYTE)
        {
            x86func_imm8(func, 0x80);
            x86func_regmem(func, dst, x86_reg(5));
            x86func_imm8(func, (uint8_t)src.offset);
        }
        else if (src.offset < INT8_MIN || src.offset > INT8_MAX)
        {
            x86func_imm8(func, 0x81);
            x86func_regmem(func, dst, x86_reg(5));
            if (func->mode == X86_MODE_REAL || opsize == X86_OPSIZE_WORD)
                x86func_imm16(func, (uint16_t)src.offset);
            else
                x86func_imm32(func, (uint32_t)src.offset);
        }
        else
        {
            x86func_imm8(func, 0x83);
            x86func_regmem(func, dst, x86_reg(5));
            x86func_imm8(func, (uint8_t)src.offset);
        }
    }
    else if (src.type == X86_REGMEM_REG)
    {
        x86func_imm8(func, opsize == X86_OPSIZE_BYTE ? 0x28 : 0x29);
        x86func_regmem(func, dst, src);
    }
    else
    {
        x86func_imm8(func, opsize == X86_OPSIZE_BYTE ? 0x2A : 0x2B);
        x86func_regmem(func, dst, src);
    }
}

void x86func_mov(x86func* func, uint8_t opsize, x86_regmem dst, x86_regmem src)
{
    if (!x86func_rex_binary(func, opsize, dst, src))
        return;

    if (src.type == X86_REGMEM_CONST)
    {
        // TODO: Use the shorter opcodes `0xB0 +r` and `0xB8 +r` for direct registers
        if (opsize == X86_OPSIZE_BYTE)
        {
            x86func_imm8(func, 0xC6);
            x86func_regmem(func, dst, x86_reg(0));
            x86func_imm8(func, (uint8_t)src.offset);
        }
        else
        {
            x86func_imm8(func, 0xC7);
            x86func_regmem(func, dst, x86_reg(0));
            if (func->mode == X86_MODE_REAL || opsize == X86_OPSIZE_WORD)
                x86func_imm16(func, (uint16_t)src.offset);
            else
                x86func_imm32(func, (uint32_t)src.offset);
        }
    }
    else if (src.type == X86_REGMEM_REG)
    {
        x86func_imm8(func, opsize == X86_OPSIZE_BYTE ? 0x88 : 0x89);
        x86func_regmem(func, dst, src);
    }
    else
    {
        x86func_imm8(func, opsize == X86_OPSIZE_BYTE ? 0x8A : 0x8B);
        x86func_regmem(func, dst, src);
    }
}

void x86func_jz(x86func* func, int32_t offset)
{
    if (offset < INT8_MIN || offset > INT8_MAX)
    {
        x86func_imm8(func, 0x0F);
        x86func_imm8(func, 0x84);
        if (func->mode >= X86_MODE_REAL)
            x86func_imm32(func, (uint32_t)offset);
        else
            x86func_imm16(func, (uint16_t)offset);
    }
    else
    {
        x86func_imm8(func, 0x74);
        x86func_imm8(func, (uint8_t)offset);
    }
}

void x86func_jnz(x86func* func, int32_t offset)
{
    if (offset < INT8_MIN || offset > INT8_MAX)
    {
        x86func_imm8(func, 0x0F);
        x86func_imm8(func, 0x85);
        if (func->mode >= X86_MODE_REAL)
            x86func_imm32(func, (uint32_t)offset);
        else
            x86func_imm16(func, (uint16_t)offset);
    }
    else
    {
        x86func_imm8(func, 0x75);
        x86func_imm8(func, (uint8_t)offset);
    }
}

void x86func_ret(x86func* func) {
    x86func_imm8(func, 0xC3);
}