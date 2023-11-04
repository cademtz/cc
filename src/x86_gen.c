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

void x86varmap_create(x86varmap* varmap)
{
    memset(varmap, 0, sizeof(*varmap));
    cc_hmap32_create(&varmap->map_perm);
    cc_hmap32_create(&varmap->map_temp);
    cc_vec_create(&varmap->vec_perm, sizeof(x86operand));
    cc_vec_create(&varmap->vec_temp, sizeof(x86operand));
}
void x86varmap_destroy(x86varmap* varmap)
{
    memset(varmap, 0, sizeof(*varmap));
    cc_hmap32_destroy(&varmap->map_perm);
    cc_hmap32_destroy(&varmap->map_temp);
    cc_vec_destroy(&varmap->vec_perm);
    cc_vec_destroy(&varmap->vec_temp);
}
void x86varmap_clear(x86varmap* varmap)
{
    cc_hmap32_clear(&varmap->map_perm);
    cc_hmap32_clear(&varmap->map_temp);
    cc_vec_clear(&varmap->vec_perm);
    cc_vec_clear(&varmap->vec_temp);
}

uint8_t x86varmap_usage(const x86varmap* map, x86operand location)
{
    x86operand* arr_perm = (x86operand*)map->vec_perm.elems;
    x86operand* arr_temp = (x86operand*)map->vec_temp.elems;

    for (size_t i = 0; i < map->vec_perm.size; ++i)
    {
        if (!x86operand_cmp(arr_perm[i], location))
            return X86_VARMAP_PERM;
    }

    for (size_t i = 0; i < map->vec_temp.size; ++i)
    {
        if (!x86operand_cmp(arr_temp[i], location))
            return X86_VARMAP_TEMP;
    }

    return X86_VARMAP_UNUSED;
}

void x86varmap_setperm(x86varmap* map, cc_ir_localid localid, x86operand location)
{
    cc_hmap32_put(&map->map_perm, localid, map->vec_perm.size);
    cc_vec_push(&map->vec_perm, &location);
}

void x86varmap_settemp(x86varmap* map, cc_ir_localid localid, x86operand location)
{
    cc_hmap32_put(&map->map_temp, localid, map->vec_temp.size);
    cc_vec_push(&map->vec_temp, &location);
}

void x86varmap_cleartemp(x86varmap* map)
{
    cc_hmap32_clear(&map->map_temp);
    cc_vec_clear(&map->vec_temp);
}

x86operand x86varmap_getperm(const x86varmap* map, cc_ir_localid localid)
{
    uint32_t index;
    const x86operand* arr = (const x86operand*)map->vec_perm.elems;
    if (!cc_hmap32_get(&map->map_perm, localid, &index))
    {
        assert(0 && "localid does not exist in map");
        return x86_offset(INT16_MAX);
    }
    return arr[index];
}

x86operand x86varmap_gettemp(const x86varmap* map, cc_ir_localid localid)
{
    uint32_t index;
    const x86operand* arr = (const x86operand*)map->vec_temp.elems;
    if (!cc_hmap32_get(&map->map_temp, localid, &index))
        return x86varmap_getperm(map, localid);
    return arr[index];
}

void x86varmap_update(x86varmap* map, cc_ir_localid localid)
{
    uint32_t vec_index;
    if (cc_hmap32_remove(&map->map_temp, localid, &vec_index))
        cc_vec_delete(&map->vec_temp, (size_t)vec_index);
}

void x86varmap_overwrite(x86varmap* map, x86operand location)
{
    cc_hmap32entry* entries = map->map_temp.entries;
    x86operand* locs = (x86operand*)map->vec_temp.elems;
    for (size_t i = 0; i < map->map_temp.num_entries; ++i)
    {
        size_t vec_index = (size_t)entries[i].value;
        cc_ir_localid localid = (cc_ir_localid)entries[i].key;
        if (x86operand_cmp(locs[vec_index], location))
            continue;
        
        cc_vec_delete(&map->vec_temp, vec_index); // Delete the location
        cc_hmap32_delete(&map->map_temp, localid); // Delete the local as a key
        // Fix indices in the map because items in vec_temp were shifted down
        for (size_t i = 0; i < map->map_temp.num_entries; ++i)
        {
            if (entries[i].value > vec_index)
                --entries[i].value;
        }
        --i;
    }
}

/**
 * @brief A primitive register allocator that (re)populates the var map and block map.
 *
 * `gen->irfunc` and `gen->func` must be set.
 */
static void x86gen_maplocals(x86gen* gen)
{
    int32_t next_var = 0;
    x86varmap_clear(&gen->varmap);
    cc_hmap32_clear(&gen->map_blocks);

    // Look for constants and "inline" them as const operands
    for (size_t i = 0; i < gen->irfunc.entry_block->num_ins; ++i)
    {
        const cc_ir_ins* ins = &gen->irfunc.entry_block->ins[i];
        if (ins->opcode == CC_IR_OPCODE_LDC)
            x86varmap_setperm(&gen->varmap, ins->write, x86_const(ins->read.u32));
    }
    
    for (size_t i = 0; i < gen->irfunc.num_locals; ++i)
    {
        const cc_ir_local* local = &gen->irfunc.locals[i];
        switch (local->localtypeid)
        {
        case CC_IR_LOCALTYPEID_BLOCK:
            cc_hmap32_put(&gen->map_blocks, local->localid, x86func_newlabel(gen->func));
            break;
        case CC_IR_LOCALTYPEID_INT:
        case CC_IR_LOCALTYPEID_PTR:
        {
            // If this local is already in the map (like the consts we added before this loop) then don't overwrite it.
            if (cc_hmap32_get_index(&gen->varmap.map_perm, local->localid) != UINT32_MAX)
                break;
            x86operand operand;
            uint8_t size = local->var_size;
            uint8_t align = 1;

            // The size of a pointer is unknown to the IR. We must clarify that here.
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
            next_var += size;
            
            operand = x86_index(X86_REG_SP, X86_REG_SP, 0, gen->stack_offset - next_var);
            x86varmap_setperm(&gen->varmap, local->localid, operand);
            break;
        }
        }
    }

    // TODO: x86varmap should be provided the size & align of locals.
    //   Then it can efficiently re-arrange locals and calculate optimal stack size for itself.
    gen->varmap.stack_size = next_var;
}

/**
 * @brief Find a volatile register
 * @param start The initial index to search. Use `0` to find the first volatile register.
 * @return A value from @ref x86_reg_enum, or @ref X86_NUM_REGISTERS
*/
static uint8_t x86gen_scratch(x86gen* gen, uint8_t start)
{
    for (uint8_t i = start; i < X86_NUM_REGISTERS; ++i)
    {
        if (gen->mode != X86_MODE_LONG)
        {
            if (i >= X86_REG_R8)
                break; // These registers are unavailable
        }
        if (i >= X86_REG_XMM0)
            break; // These registers are too annoying to use (for now)
        if (gen->conv->volatiles[i])
            return i;
    }
    return X86_NUM_REGISTERS;
}

/// @brief Load the permanent location of a local and invalidate any copies
static x86operand x86gen_load_writeable(x86gen* gen, cc_ir_localid localid)
{
    x86varmap_update(&gen->varmap, localid);
    x86operand loc = x86varmap_getperm(&gen->varmap, localid);
    if (loc.type == X86_OPERAND_MEM && loc.index == X86_REG_SP)
        loc.offset -= gen->stack_offset; // Adjust for current stack offset
    return loc;
}

/// @brief Load the most convenient copy of a local (prefers registers and consts over memory)
static x86operand x86gen_load_readable(x86gen* gen, cc_ir_localid localid)
{
    x86operand loc = x86varmap_gettemp(&gen->varmap, localid);
    if (loc.type == X86_OPERAND_MEM && loc.index == X86_REG_SP)
        loc.offset -= gen->stack_offset; // Adjust for current stack offset
    return loc;
}

/// @brief Load a local to a register or const value, emitting code when necessary.
static x86operand x86gen_load_restricted(x86gen* gen, cc_ir_localid localid)
{
    x86operand source = x86gen_load_readable(gen, localid);
    if (source.type == X86_OPERAND_REG || source.type == X86_OPERAND_CONST)
        return source;
    
    // The first unused register, or X86_NUM_REGISTERS
    uint8_t unused_reg = x86gen_scratch(gen, 0);
    // The last (temporarily) used register, or X86_NUM_REGISTERS
    uint8_t temp_reg = X86_NUM_REGISTERS;

    while (unused_reg < X86_NUM_REGISTERS)
    {
        uint8_t usage = x86varmap_usage(&gen->varmap, x86_reg(unused_reg));
        if (usage == X86_VARMAP_UNUSED)
            break;
        if (usage == X86_VARMAP_TEMP)
            temp_reg = unused_reg;
        unused_reg = x86gen_scratch(gen, unused_reg + 1);
    }

    uint8_t reg = unused_reg;
    if (reg >= X86_NUM_REGISTERS)
    {
        reg = temp_reg;
        if (reg >= X86_NUM_REGISTERS)
        {
            assert(0 && "Could not allocate a volatile register");
            return x86_const(INT16_MAX);
        }
    }

    x86operand dst = x86_reg(reg);
    x86func_mov(gen->func, 0, dst, source);
    x86varmap_overwrite(&gen->varmap, dst);
    x86varmap_settemp(&gen->varmap, localid, dst);
    return x86_reg(reg);
}

void x86gen_create(x86gen* gen, const x86conv* conv, const cc_ir_func* irfunc)
{
    memset(gen, 0, sizeof(*gen));
    gen->mode = X86_MODE_PROTECTED;
    gen->conv = conv;
    // Make the required transformations for easier code gen
    x86gen_simplify(gen, irfunc, &gen->irfunc);
    x86varmap_create(&gen->varmap);
}

void x86gen_destroy(x86gen* gen)
{
    cc_hmap32_destroy(&gen->map_blocks);
    x86varmap_destroy(&gen->varmap);
}

void x86gen_simplify(const x86gen* gen, const cc_ir_func* input, cc_ir_func* output)
{
    cc_ir_func_clone(input, output);
    cc_ir_block* block = output->entry_block;
    for (; block; block = block->next_block)
    {
        for (size_t i = 0; i < block->num_ins; ++i)
        {
            cc_ir_ins* ins = &block->ins[i];
            switch (ins->opcode)
            {
            // Order-independent
            case CC_IR_OPCODE_ADD:
                if (ins->write == ins->read.local[1])
                {
                    // Convert:
                    // add x, y, x ->
                    // add x, x, y
                    ins->read.local[1] = ins->read.local[0];
                    ins->read.local[0] = ins->write;
                    break;
                }
            // Order-dependent
            case CC_IR_OPCODE_SUB:
                if (ins->write != ins->read.local[0])
                {
                    // Convert:
                    // sub dst, lhs, rhs ->
                    // mov dst, lhs      ; dst = lhs
                    // sub dst, dst, rhs ; dst += rhs
                    cc_ir_localid dst = ins->write;
                    cc_ir_localid lhs = ins->read.local[0];
                    cc_ir_localid rhs = ins->read.local[1];
                    ins->read.local[0] = dst; // sub dst, lhs, rhs -> sub dst, dst, rhs
                    cc_ir_block_insert(block, i, cc_ir_ins_mov(dst, lhs));
                    ++i;
                }
                break;
            }
            // Do not reference `ins` at this point. It may have been relocated.
        }
    }
}

void x86gen_dump(x86gen* gen, x86func* func)
{
    x86label return_label;
    int32_t next_var = 0;
    gen->stack_offset = 0;
    gen->func = func;
    cc_hmap32_clear(&gen->map_blocks);
    x86func_create(func, gen->mode);
    x86gen_maplocals(gen);
    return_label = x86func_newlabel(func);

    uint32_t reserved_stack = CC_ALIGN(gen->varmap.stack_size, x86_ptrsize(gen->mode));
    if (reserved_stack)
        x86func_sub(func, 0, x86_reg(X86_REG_SP), x86_const(reserved_stack));
    gen->stack_offset -= reserved_stack;
    
    for (const cc_ir_block* block = gen->irfunc.entry_block; block; block = block->next_block)
    {
        x86func_label(func, cc_hmap32_get_default(&gen->map_blocks, block->localid, INT16_MAX));
        x86varmap_cleartemp(&gen->varmap);
        for (size_t i = 0; i < block->num_ins; ++i)
        {
            const cc_ir_ins* ins = &block->ins[i];
            const x86operand scratch = x86_reg(x86gen_scratch(gen, 0));
            x86operand dst = x86_offset(INT16_MAX);
            x86operand src[2] = {x86_offset(INT16_MAX), x86_offset(INT16_MAX)};
            uint32_t src_size[2] = {0,0};

            // Get src_size
            switch (ins->opcode)
            {
            // Two sources
            case CC_IR_OPCODE_MOV:
            case CC_IR_OPCODE_ADD:
            case CC_IR_OPCODE_SUB:
            case CC_IR_OPCODE_JNZ:
            {
                src_size[1] = cc_ir_func_getlocal(&gen->irfunc, ins->read.local[1])->var_size;
                if (src_size[1] == 0)
                    src_size[1] = x86_ptrsize(gen->mode);
                if (ins->opcode == CC_IR_OPCODE_JNZ)
                    break;
            }
            // One source
            case CC_IR_OPCODE_RETL:
            {
                src_size[0] = cc_ir_func_getlocal(&gen->irfunc, ins->read.local[0])->var_size;
                if (src_size[0] == 0)
                    src_size[0] = x86_ptrsize(gen->mode);
                break;
            }
            }

            switch (ins->opcode)
            {
            // LDC is not needed, because consts are inlined as operands.
            //case CC_IR_OPCODE_LDC:
            case CC_IR_OPCODE_LDLS:
                dst = x86gen_load_writeable(gen, ins->write);
                x86func_mov(func, 0, dst, x86_const(src_size[0]));
                break;
            case CC_IR_OPCODE_MOV:
                dst = x86gen_load_writeable(gen, ins->write);
                src[0] = x86gen_load_restricted(gen, ins->read.local[0]);
                x86func_mov(func, 0, dst, src[0]);
                break;
            case CC_IR_OPCODE_ADD:
                dst = x86gen_load_writeable(gen, ins->write);
                src[1] = x86gen_load_restricted(gen, ins->read.local[1]);
                x86func_add(func, 0, dst, src[1]);
                break;
            case CC_IR_OPCODE_SUB:
                dst = x86gen_load_writeable(gen, ins->write);
                src[1] = x86gen_load_restricted(gen, ins->read.local[1]);
                x86func_sub(func, 0, dst, src[1]);
                break;
            case CC_IR_OPCODE_JMP:
                x86func_jmp(func, cc_hmap32_get_default(&gen->map_blocks, ins->read.local[0], INT16_MAX));
                break;
            case CC_IR_OPCODE_JNZ:
                // src[0] is the block, and src[1] is the actual value to test
                x86func_cmp(func, 0, x86gen_load_readable(gen, ins->read.local[1]), x86_const(0));
                x86func_jnz(func, cc_hmap32_get_default(&gen->map_blocks, ins->read.local[0], INT16_MAX));
                break;
            case CC_IR_OPCODE_RETL:
                // FIXME: Add mapping functions to x86conv instead of hardcoding RAX.
                // We could use `x86conv_return(conv, data_type, data_size)` to find where a returned value must go.
                x86func_mov(func, 0, x86_reg(X86_REG_A), x86gen_load_readable(gen, ins->read.local[0]));
            case CC_IR_OPCODE_RET:
                if (block->next_block)
                    x86func_jmp(func, return_label);
                break;
            }
        }
    }

    x86func_label(func, return_label);
    if (gen->stack_offset)
        x86func_add(func, 0, x86_reg(X86_REG_SP), x86_const(-gen->stack_offset));
    x86func_ret(func);

    gen->stack_offset = 0;
    gen->func = NULL;
}