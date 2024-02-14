#include "test.h"
#include <cc/ir.h>
#include <stdio.h>
#include <cc/x86_gen.h>

int test_block(void)
{
    cc_ir_object obj;
    cc_ir_object_create(&obj);

    cc_ir_func* irfunc = cc_ir_object_add_func(&obj, NULL, -1);

    cc_ir_block* entry = irfunc->entry_block;
    cc_ir_block* loop = cc_ir_func_insert(irfunc, entry, CC_STR("loop"), -1);
    cc_ir_block* end = cc_ir_func_insert(irfunc, loop, CC_STR("end"), -1);

    const int INT_SIZE = 4;

    cc_ir_block_iconst(entry, INT_SIZE, 9);
    cc_ir_block_iconst(entry, INT_SIZE, 10);
    cc_ir_block_add(entry, INT_SIZE);

    cc_ir_block_ret(end);

    printf("Original IR:\n");
    print_ir_func(irfunc);

    cc_ir_object_destroy(&obj);
    return 1;
}