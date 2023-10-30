#include <cc/x86_gen.h>
#include <cc/ir.h>

const x86conv X86_CONV_WIN64_FASTCALL =
{
    {
        1, 1, 1, 0, // a, c, d, b
        0, 0, 0, 0, // sp, bp, si, di

        1, 1, 1, 1, // r8, r9, r10, r11
        0, 0, 0, 0, // r12, r13, r14, r15

        1, 1, 1, 1, // xmm0, xmm1, xmm2, xmm3
        1, 1, 0, 0, // xmm4, xmm5, xmm6, xmm7

        0, 0, 0, 0, // xmm8, xmm9, xmm10, xmm11
        0, 0, 0, 0, // xmm12, xmm13, xmm14, xmm15
    },
    32,    // stack_preargs
    0,     // stack_postargs
    false, // noreturn
};
const x86conv X86_CONV_SYSV64_CDECL =
{
    {
        1, 1, 1, 0, // a, c, d, b
        0, 0, 1, 1, // sp, bp, si, di

        1, 1, 1, 1, // r8, r9, r10, r11
        0, 0, 0, 0, // r12, r13, r14, r15

        0, 0, 0, 0, // xmm0, xmm1, xmm2, xmm3
        0, 0, 0, 0, // xmm4, xmm5, xmm6, xmm7

        0, 0, 0, 0, // xmm8, xmm9, xmm10, xmm11
        0, 0, 0, 0, // xmm12, xmm13, xmm14, xmm15
    },
    0,     // stack_preargs
    0,     // stack_postargs
    false, // noreturn
};

void x86gen_create(x86gen* gen, const x86conv* conv, const cc_ir_func* irfunc)
{
    memset(gen, 0, sizeof(*gen));
    gen->mode = X86_MODE_PROTECTED;
    gen->conv = conv;
    gen->irfunc = irfunc;
}

void x86gen_destroy(x86gen* gen) {}

void x86gen_dump(x86gen* gen, x86func* func)
{
    int32_t stack_offset = 0;
    x86label return_label;
    cc_hmap32 map_vars;   // localid -> stack offset
    cc_hmap32 map_blocks; // localid -> x86label
    int32_t next_var = 0;

    cc_hmap32_create(&map_vars);
    cc_hmap32_create(&map_blocks);
    x86func_create(func, gen->mode);
    return_label = x86func_newlabel(func);

    for (size_t i = 0; i < gen->irfunc->num_locals; ++i)
    {
        const cc_ir_local* local = &gen->irfunc->locals[i];
        switch (local->localtypeid)
        {
        case CC_IR_LOCALTYPEID_BLOCK:
            cc_hmap32_put(&map_blocks, local->localid, x86func_newlabel(func));
            break;
        case CC_IR_LOCALTYPEID_INT:
        case CC_IR_LOCALTYPEID_PTR:
        {
            uint8_t size = local->var_size;
            uint8_t align = 1;
            if (local->localtypeid == CC_IR_LOCALTYPEID_PTR)
                size = x86_ptrsize(gen->mode);
            if (size >= 16)
                align = 16;
            else if (size >= 8)
                align = 8;
            else if (size >= 4)
                align = 4;
            else if (size >= 2)
                align = 2;
            next_var = CC_ALIGN(next_var, align);
            cc_hmap32_put(&map_vars, local->localid, -next_var);
            next_var += size;
            break;
        }
        }
    }

    next_var = CC_ALIGN(next_var, x86_ptrsize(gen->mode));

    x86func_sub(func, 0, x86_reg(X86_REG_SP), x86_const(next_var));
    stack_offset -= next_var;
    
    for (const cc_ir_block* block = gen->irfunc->entry_block; block; block = block->next_block)
    {
        x86func_label(func, cc_hmap32_get_default(&map_blocks, block->localid, INT16_MAX));
        for (size_t i = 0; i < block->num_ins; ++i)
        {
            const cc_ir_ins* ins = &block->ins[i];
            x86operand dst = x86_offset(INT16_MAX);
            x86operand src[2] = {x86_offset(INT16_MAX), x86_offset(INT16_MAX)};
            uint32_t dst_size = 0;
            uint32_t src_size[2] = {0,0};

            // Get src and src_size
            switch (ins->opcode)
            {
            case CC_IR_OPCODE_LDC:
            case CC_IR_OPCODE_LDLS:
            case CC_IR_OPCODE_ADD:
            case CC_IR_OPCODE_SUB:
            {
                int32_t var_offset = cc_hmap32_get_default(&map_vars, ins->write, 0x7FFF);
                dst = x86_index(X86_REG_SP, X86_REG_SP, X86_SIB_SCALE_1, var_offset - stack_offset);
                dst_size = cc_ir_func_getlocal(gen->irfunc, ins->write)->var_size;
                if (dst_size == 0)
                    dst_size = x86_ptrsize(gen->mode);
                break;
            }
            }

            // Get read_offset and read_size
            switch (ins->opcode)
            {
            case CC_IR_OPCODE_ADD:
            case CC_IR_OPCODE_SUB:
            case CC_IR_OPCODE_JNZ:
            {
                uint32_t var_offset = cc_hmap32_get_default(&map_vars, ins->read.local[1], 0x7FFF);
                src[1] = x86_index(X86_REG_SP, X86_REG_SP, X86_SIB_SCALE_1, var_offset - stack_offset);
                src_size[1] = cc_ir_func_getlocal(gen->irfunc, ins->read.local[1])->var_size;
                if (src_size[1] == 0)
                    src_size[1] = x86_ptrsize(gen->mode);
                if (ins->opcode == CC_IR_OPCODE_JNZ)
                    break;
            }
            case CC_IR_OPCODE_RETL:
            {
                uint32_t var_offset = cc_hmap32_get_default(&map_vars, ins->read.local[0], 0x7FFF);
                src[0] = x86_index(X86_REG_SP, X86_REG_SP, X86_SIB_SCALE_1, var_offset - stack_offset);
                src_size[0] = cc_ir_func_getlocal(gen->irfunc, ins->read.local[0])->var_size;
                if (src_size[0] == 0)
                    src_size[0] = x86_ptrsize(gen->mode);
                break;
            }
            }

            switch (ins->opcode)
            {
            case CC_IR_OPCODE_LDC:
                x86func_mov(func, 0, dst, x86_const(ins->read.u32));
                break;
            case CC_IR_OPCODE_LDLS:
                x86func_mov(func, 0, dst, x86_const(src_size[0]));
                break;
            case CC_IR_OPCODE_ADD:
                // FIXME: Choose a volatile register based on the calling convention, instead of hardcoding RAX.
                // TODO: Avoid moving values if sources or destinations are the same
                x86func_mov(func, 0, x86_reg(X86_REG_A), src[0]);
                x86func_add(func, 0, x86_reg(X86_REG_A), src[1]);
                x86func_mov(func, 0, dst, x86_reg(X86_REG_A));
                break;
            case CC_IR_OPCODE_SUB:
                x86func_mov(func, 0, x86_reg(X86_REG_A), src[0]);
                x86func_sub(func, 0, x86_reg(X86_REG_A), src[1]);
                x86func_mov(func, 0, dst, x86_reg(X86_REG_A));
                break;
            case CC_IR_OPCODE_JNZ:
                // src[0] is the block, and src[1] is the actual value to test
                x86func_cmp(func, 0, src[1], x86_const(0));
                x86func_jnz(func, cc_hmap32_get_default(&map_blocks, ins->read.local[0], INT16_MAX));
                break;
            case CC_IR_OPCODE_RETL:
                x86func_mov(func, 0, x86_reg(X86_REG_A), src[0]);
            case CC_IR_OPCODE_RET:
                x86func_jmp(func, return_label);
                break;
            }
        }
    }

    x86func_label(func, return_label);
    x86func_add(func, 0, x86_reg(X86_REG_SP), x86_const(-stack_offset));
    x86func_ret(func);

    cc_hmap32_destroy(&map_blocks);
    cc_hmap32_destroy(&map_vars);
}