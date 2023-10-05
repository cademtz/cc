#include "test.h"
#include <cc/ir.h>

static void print_ir_local(const cc_ir_func* func, cc_ir_localid localid)
{
    const cc_ir_local* local = &func->locals[localid];
    printf(" ");
    switch (local->localtypeid)
    {
    case CC_IR_LOCALTYPEID_BLOCK: printf("block "); break;
    case CC_IR_LOCALTYPEID_FUNC: printf("func "); break;
    case CC_IR_LOCALTYPEID_INT:
        printf("i%d ", local->var_size * 8);
        break;
    }
    if (local->name)
        printf("%s", local->name);
    else
        printf("local_%d");
    printf(",");
}

static void print_ir_func(const cc_ir_func* func)
{
    for (size_t b = 0; b < func->num_blocks; ++b)
    {
        const cc_ir_block* block = &func->blocks[b];
        printf("block #%zu:\n", b);
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
                else if (i == 1)
                {
                    if (fmt->operand[i] == CC_IR_OPERAND_U32)
                        printf(" %u,", ins->read.u32);
                    else if (fmt->operand[i] == CC_IR_OPERAND_LOCAL)
                        print_ir_local(func, ins->read.local[0]);
                }
                else if (i == 2)
                    print_ir_local(func, ins->read.local[1]);
                else
                    printf(" <unknown operand>,");
            }
            printf("\n");
        }
    }
}

int test_block(void)
{
    cc_ir_func func;
    cc_ir_func_create(&func);

    cc_ir_block* my_block = cc_ir_func_block(&func, "my_block");
    cc_ir_localid my_int = cc_ir_func_int(&func, 4, "my_int");
    cc_ir_localid negative_int = cc_ir_func_int(&func, 4, "negative_int");

    cc_ir_block_ldc(my_block, my_int, 9);
    cc_ir_block_ldc(my_block, negative_int, (uint32_t)-4);
    cc_ir_block_add(my_block, my_int, my_int, negative_int);

    print_ir_func(&func);

    cc_ir_func_destroy(&func);
    return 1;
}