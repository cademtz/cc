#include <cc/x86_gen.h>

/// @brief Reserve `num_bytes` bytes in `func->code` for writing and advance `func->writepos`
/// @return A temporary pointer to the reserved bytes
static uint8_t* x86func_reserve(x86func* func, size_t num_bytes)
{
    size_t min_size = func->writepos + num_bytes;
    if (func->size_code < min_size)
    {
        func->size_code = min_size;
        func->code = (uint8_t*)realloc(func->code, func->size_code);
    }
    
    uint8_t* result = func->code + func->writepos;
    func->writepos += num_bytes;
    return result;
}

/**
 * @brief Emit an immediate value in code and save the location
 * @param value The immediate value to emit
 * @param size Size of the offset, in bytes. Must be 1, 2, or 4.
 * @param out_info Store info about the new immediate value
 */
static void x86func_imm(x86func* func, uint32_t value, uint8_t size, x86imm* out_info)
{
    size_t offset = func->size_code;

    switch (size)
    {
    case 1: x86func_imm8(func, (uint8_t)value); break;
    case 2: x86func_imm16(func, (uint16_t)value); break;
    case 4: x86func_imm32(func, (uint32_t)value); break;
    default:
        assert(0 && "Size must be a power of two, less-than or equal to 4");
    }
    
    out_info->offset = offset;
    out_info->size = size;
}

/**
 * @brief Emit the encoded reg/mem and reg operands.
 * 
 * If lhs/rhs is a memory offset (such as `[0x1000]`), then `func->lhs_imm`/`func->rhs_imm` is written.
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
    x86imm* imm_info = NULL;// Store info about the immediate, if any
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
            imm_info = !lhs_direct ? &func->lhs_imm : &func->rhs_imm;
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

    if (imm_info)
    {
        imm_info->offset = func->size_code - imm_size;
        imm_info->size = imm_size;
    }
    return true;
}

/**
 * @brief Emit various operand-affecting prefixes (REX and others) where necessary.
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
        assert(0 && "This execution mode is not implemented");
    }

    if (rex)
        x86func_imm8(func, rex | X86_REX__REX);
    return true;
}

void x86func_create(x86func* func, uint8_t mode)
{
    memset(func, 0, sizeof(*func));
    func->mode = mode;
}
void x86func_destroy(x86func* func)
{
    free(func->code);
    free(func->labels);
    free(func->labelrefs);
}
x86label x86func_newlabel(x86func* func)
{
    x86label index = func->num_labels++;
    func->labels = (uint32_t*)realloc(func->labels, func->num_labels * sizeof(func->labels[0]));
    func->labels[index] = UINT32_MAX;
    return index;
}
/// @brief Place `label` at `offset` and modify every reference in code
static void x86func__movelabel(x86func* func, x86label label, uint32_t offset)
{
    size_t old_writepos = func->writepos;
    func->labels[label] = offset;

    for (size_t i = 0; i < func->num_labelrefs; ++i)
    {
        const x86labelref* ref = &func->labelrefs[i];
        if (ref->label != label)
            continue;
        
        int32_t new_offset = offset - ref->next_ip;
        func->writepos = ref->imm.offset;
        switch (ref->imm.size)
        {
        case 1: x86func_imm8(func, (uint8_t)new_offset); break;
        case 2: x86func_imm16(func, (uint16_t)new_offset); break;
        case 4: x86func_imm32(func, (uint32_t)new_offset); break;
        default:
            assert(0 && "Size of a labelref's immediate must be 1, 2, or 4 bytes");
        }
    }
    func->writepos = old_writepos;
}
void x86func_label(x86func* func, x86label label) {
    x86func__movelabel(func, label, func->size_code);
}
/// @brief Save the current IP with `imm` as a reference to `label`
void x86func__labelref(x86func* func, x86label label, const x86imm* imm)
{
    ++func->num_labelrefs;
    func->labelrefs = (x86labelref*)realloc(func->labelrefs, func->num_labelrefs * sizeof(func->labelrefs[0]));

    x86labelref* ref = &func->labelrefs[func->num_labelrefs - 1];
    memset(ref, 0, sizeof(*ref));
    ref->imm = *imm;
    ref->next_ip = func->size_code;
    ref->label = label;
}

void x86func_imm8(x86func* func, uint8_t byte) {
    *x86func_reserve(func, 1) = byte;
}
void x86func_imm16(x86func* func, uint16_t imm)
{
    uint8_t* dst = x86func_reserve(func, sizeof(imm));
    for (int i = 0; i < sizeof(imm); ++i)
        dst[i] = (imm >> (i * 8)) & 0xFF;
}
void x86func_imm32(x86func* func, uint32_t imm)
{
    uint8_t* dst = x86func_reserve(func, sizeof(imm));
    for (int i = 0; i < sizeof(imm); ++i)
        dst[i] = (imm >> (i * 8)) & 0xFF;
}
void x86func_imm64(x86func* func, uint64_t imm)
{
    uint8_t* dst = x86func_reserve(func, sizeof(imm));
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
            uint8_t imm_size = (func->mode == X86_MODE_REAL || opsize == X86_OPSIZE_WORD) ? 2 : 4;
            x86func_imm(func, src.offset, imm_size, &func->rhs_imm);
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
            uint8_t imm_size = (func->mode == X86_MODE_REAL || opsize == X86_OPSIZE_WORD) ? 2 : 4;
            x86func_imm(func, src.offset, imm_size, &func->rhs_imm);
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

void x86func_imul(x86func* func, uint8_t opsize, x86_regmem src)
{
    if (src.type == X86_REGMEM_CONST)
        return;
    if (!x86func_rex_binary(func, opsize, src, x86_reg(5)))
        return;
    x86func_imm8(func, opsize == X86_OPSIZE_BYTE ? 0xF6 : 0xF7);
    x86func_regmem(func, src, x86_reg(5));
}

void x86func_imul2(x86func* func, uint8_t opsize, uint8_t dst, x86_regmem src)
{
    if (src.type == X86_REGMEM_CONST)
    {
        x86func_imul3(func, opsize, dst, x86_reg(dst), src.offset);
        return;
    }

    if (opsize == X86_OPSIZE_BYTE)
        return;

    // dst must be encoded as ModRM.reg.
    // To ensure this, dst is passed as the "rhs" operand to our helpers.
    // Helpers ensure "rhs" is always encode as ModRM.reg (if it's direct).
    // Also with src as "lhs", src's immediates will be placed in `lhs_imm` for us!

    if (!x86func_rex_binary(func, opsize, src, x86_reg(dst)))
        return;
    
    x86func_imm8(func, 0x0F);
    x86func_imm8(func, 0xAF);
    x86func_regmem(func, src, x86_reg(dst));
}

void x86func_imul3(x86func* func, uint8_t opsize, uint8_t dst, x86_regmem lhs, int32_t rhs)
{
    if (opsize == X86_OPSIZE_BYTE)
        return;

    if (!x86func_rex_binary(func, opsize, lhs, x86_reg(dst)))
        return;
    
    if (rhs < INT8_MIN || rhs > INT8_MAX)
    {
        x86func_imm8(func, 0x69);
        x86func_regmem(func, lhs, x86_reg(dst));
        uint8_t imm_size = (func->mode == X86_MODE_REAL || opsize == X86_OPSIZE_WORD) ? 2 : 4;
        x86func_imm(func, (uint32_t)rhs, imm_size, &func->rhs_imm);
    }
    else
    {
        x86func_imm8(func, 0x6B);
        x86func_regmem(func, lhs, x86_reg(dst));
        x86func_imm8(func, (uint8_t)rhs);
    }
}

void x86func_cmp(x86func* func, uint8_t opsize, x86_regmem lhs, x86_regmem rhs)
{
    if (lhs.type == X86_REGMEM_CONST)
        return;

    if (!x86func_rex_binary(func, opsize, lhs, rhs))
        return;

    if (rhs.type == X86_REGMEM_CONST)
    {
        if (opsize == X86_OPSIZE_BYTE)
        {
            x86func_imm8(func, 0x80);
            x86func_regmem(func, lhs, x86_reg(7));
            x86func_imm8(func, (uint8_t)rhs.offset);
        }
        else if (rhs.offset < INT8_MIN || rhs.offset > INT8_MAX)
        {
            x86func_imm8(func, 0x81);
            x86func_regmem(func, lhs, x86_reg(7));
            uint8_t imm_size = (func->mode == X86_MODE_REAL || opsize == X86_OPSIZE_WORD) ? 2 : 4;
            x86func_imm(func, rhs.offset, imm_size, &func->rhs_imm);
        }
        else
        {
            x86func_imm8(func, 0x83);
            x86func_regmem(func, lhs, x86_reg(7));
            x86func_imm8(func, (uint8_t)rhs.offset);
        }
    }
    else if (rhs.type == X86_REGMEM_REG)
    {
        x86func_imm8(func, opsize == X86_OPSIZE_BYTE ? 0x38 : 0x39);
        x86func_regmem(func, lhs, rhs);
    }
    else
    {
        x86func_imm8(func, opsize == X86_OPSIZE_BYTE ? 0x3A : 0x3B);
        x86func_regmem(func, lhs, rhs);
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

/// @brief Internal jcc impl
/// @param offset Offset relative to the end of the instruction
/// @param nibble The lower opcode nibble, as defined in the AMD64 manual
static void x86func__jcc(x86func* func, int32_t offset, uint8_t nibble)
{
    if (offset < INT8_MIN || offset > INT8_MAX)
    {
        x86func_imm8(func, 0x0F);
        x86func_imm8(func, 0x80 | nibble);
        x86func_imm(func, offset, func->mode >= X86_MODE_PROTECTED ? 4 : 2, &func->lhs_imm);
    }
    else
    {
        x86func_imm8(func, 0x70 | nibble);
        x86func_imm8(func, (uint8_t)offset);
    }
}

/// @brief Internal jcc impl, except `offset` is simpler to use
/// @param offset Offset relative to the start of the instruction
/// @param nibble The lower opcode nibble, as defined in the AMD64 manual
static void x86func__jcc_simpler(x86func* func, int32_t offset, uint8_t nibble)
{
    // This new offset is relative to the end of the instruction.
    // Assume the instruction will be 2 bytes long
    int32_t post_offset = offset - 2;
    size_t ins_begin = func->size_code;
    x86func__jcc(func, post_offset, nibble);

    // The offset does not fit in 1 byte. A larger instruction was emitted.
    if (post_offset < INT8_MIN || post_offset > INT8_MAX)
    {
        // Rewrite the offset, adjusted using the new instruction size.
        // We know the immediate is emitted at the end, and only 16- or 32-bit.
        size_t ins_size = func->size_code - ins_begin;
        post_offset = offset - ins_size;
        func->writepos = func->lhs_imm.offset;
        if (func->lhs_imm.size == 2)
            x86func_imm16(func, (uint16_t)offset);
        else
            x86func_imm32(func, (uint32_t)offset);
    }
}

/// @brief Higher level internal jcc impl, which jumps/references to a label
/// @param nibble The lower opcode nibble, as defined in the AMD64 manual
static void x86func__jcc_label(x86func* func, x86label label, uint8_t nibble)
{
    if (func->labels[label] != UINT32_MAX) // The label is defined
        x86func__jcc_simpler(func, (int32_t)(func->labels[label] - func->size_code), nibble);
    else  // The label is undefined. Reserve a 16-bit jump (expands to 32-bit based on execution mode)
    {
        x86func__jcc(func, INT16_MAX, nibble);
        x86func__labelref(func, label, &func->lhs_imm);
    }
}

void x86func_jo(x86func* func, x86label label) {
    x86func__jcc_label(func, label, 0);
}
void x86func_jno(x86func* func, x86label label) {
    x86func__jcc_label(func, label, 1);
}
void x86func_jc(x86func* func, x86label label) {
    x86func__jcc_label(func, label, 2);
}
void x86func_jnc(x86func* func, x86label label) {
    x86func__jcc_label(func, label, 3);
}
void x86func_jz(x86func* func, x86label label) {
    x86func__jcc_label(func, label, 4);
}
void x86func_jnz(x86func* func, x86label label) {
    x86func__jcc_label(func, label, 5);
}
void x86func_jbe(x86func* func, x86label label) {
    x86func__jcc_label(func, label, 6);
}
void x86func_jnbe(x86func* func, x86label label) {
    x86func__jcc_label(func, label, 7);
}
void x86func_js(x86func* func, x86label label) {
    x86func__jcc_label(func, label, 8);
}
void x86func_jns(x86func* func, x86label label) {
    x86func__jcc_label(func, label, 9);
}
void x86func_jp(x86func* func, x86label label) {
    x86func__jcc_label(func, label, 0xA);
}
void x86func_jnp(x86func* func, x86label label) {
    x86func__jcc_label(func, label, 0xB);
}
void x86func_jl(x86func* func, x86label label) {
    x86func__jcc_label(func, label, 0xC);
}
void x86func_jnl(x86func* func, x86label label) {
    x86func__jcc_label(func, label, 0xD);
}
void x86func_jle(x86func* func, x86label label) {
    x86func__jcc_label(func, label, 0xE);
}
void x86func_jnle(x86func* func, x86label label) {
    x86func__jcc_label(func, label, 0xF);
}

void x86func_ret(x86func* func) {
    x86func_imm8(func, 0xC3);
}