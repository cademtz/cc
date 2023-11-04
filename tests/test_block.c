#include "test.h"
#include <cc/ir.h>
#include <stdio.h>
#include <cc/x86_gen.h>

static void print_ir_local(const cc_ir_func* func, cc_ir_localid localid)
{
    const cc_ir_local* local = cc_ir_func_getlocal(func, localid);
    printf(" ");
    switch (local->localtypeid)
    {
    case CC_IR_LOCALTYPEID_BLOCK: printf("block "); break;
    case CC_IR_LOCALTYPEID_FUNC: printf("func "); break;
    case CC_IR_LOCALTYPEID_INT:
        printf("i%d ", local->var_size * 8);
        break;
    case CC_IR_LOCALTYPEID_PTR: printf("ptr "); break;
    default:
        printf("<unknown local type> ");
    }
    if (local->name)
        printf("%s", local->name);
    else if (local->localid == cc_ir_func_getlocalself(func))
        printf("<current function>");
    else
        printf("local_%d", localid);
    printf(",");
}

static void print_ir_func(const cc_ir_func* func)
{
    for (const cc_ir_block* block = func->entry_block; block; block = block->next_block)
    {
        const cc_ir_local* block_local = cc_ir_func_getlocal(func, block->localid);
        if (block_local->name)
            printf("%s:\n", block_local->name);
        else
            printf("local_%d:\n", block_local->localid);

        for (size_t i = 0; i < block->num_ins; ++i)
        {
            const cc_ir_ins* ins = &block->ins[i];
            const cc_ir_ins_format* fmt = &cc_ir_ins_formats[ins->opcode];
            printf("  %s", fmt->mnemonic);
            for (int i = 0; i < CC_IR_MAX_OPERANDS; ++i) {
                if (!fmt->operand[i])
                    continue;

                if (i == 0) // Write operand
                    print_ir_local(func, ins->write);
                else
                {
                    switch (fmt->operand[i])
                    {
                    case CC_IR_OPERAND_U32: printf(" %u,", ins->read.u32); break;
                    case CC_IR_OPERAND_LOCAL:
                    {
                        int read_index = i - 1;
                        if (read_index >= 0 && read_index <= 1)
                            print_ir_local(func, ins->read.local[read_index]);
                        else
                            printf(" <invalid read index>,");
                        break;
                    }
                    default: printf(" <unknown operand>,"); break;
                    }
                }
            }
            printf("\n");
        }
    }
}

int test_block(void)
{
    cc_ir_func irfunc;
    cc_ir_func_create(&irfunc, CC_STR("my_func"));

    cc_ir_block* entry = irfunc.entry_block;
    cc_ir_block* loop = cc_ir_func_insert(&irfunc, entry, CC_STR("loop"));
    cc_ir_block* end = cc_ir_func_insert(&irfunc, loop, CC_STR("end"));

    // Create some local variables
    cc_ir_localid positive = cc_ir_func_int(&irfunc, 4, CC_STR("positive"));
    cc_ir_localid negative = cc_ir_func_int(&irfunc, 4, CC_STR("negative"));
    cc_ir_localid result = cc_ir_func_int(&irfunc, 4, CC_STR("result"));

    // Program the entry block
    cc_ir_block_ldc(entry, positive, 9);                // positive = 9
    cc_ir_block_ldc(entry, negative, (uint32_t)-4);     // negative = -4
    cc_ir_block_add(entry, result, positive, negative); // result = positive + negative
    
    cc_ir_block_add(loop, result, result, positive);    // result += positive
    cc_ir_block_add(loop, result, result, negative);    // result += negative
    cc_ir_block_jnz(loop, loop, result);                // if (result != 0) goto end

    // Program the end block
    cc_ir_block_retl(end, result);                      // return result

    x86gen gen;
    x86gen_create(&gen, &X86_CONV_SYSV64_CDECL, &irfunc);
    gen.mode = X86_MODE_PROTECTED;

    // Simplify the IR for x86 and see what it looks like
    cc_ir_func simplified;
    x86gen_simplify(&gen, &irfunc, &simplified);
    printf("Original IR:\n");
    print_ir_func(&irfunc);
    printf("x86-like IR:\n");
    print_ir_func(&simplified);
    cc_ir_func_destroy(&simplified);
    
    x86func compiled;
    x86gen_dump(&gen, &compiled);
    printf("x86:");
    for (size_t i = 0; i < compiled.size_code; ++i)
        printf(" %.2X", compiled.code[i]);
    printf("\n");
    x86func_destroy(&compiled);
    
    x86gen_destroy(&gen);
    cc_ir_func_destroy(&irfunc);
    return 1;
}