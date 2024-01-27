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
        printf("i%d ", local->data_size * 8);
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

                switch (fmt->operand[i])
                {
                case CC_IR_OPERAND_U32: printf(" u32:%u,", ins->operand.u32); break;
                case CC_IR_OPERAND_LOCAL: print_ir_local(func, ins->operand.local); break;
                case CC_IR_OPERAND_DATASIZE: printf(" size:%u,", ins->data_size); break;
                default: printf(" <unknown operand>,"); break;
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

    const int INT_SIZE = 4;

    cc_ir_block_iconst(entry, INT_SIZE, 9);
    cc_ir_block_iconst(entry, INT_SIZE, 10);
    cc_ir_block_add(entry, INT_SIZE);

    cc_ir_block_ret(end);

    printf("Original IR:\n");
    print_ir_func(&irfunc);

    cc_ir_func_destroy(&irfunc);
    return 1;
}