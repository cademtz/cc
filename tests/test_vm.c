#include "test.h"
#include <stdio.h>
#include <cc/vm.h>
#include <cc/ir.h>

int test_vm(void)
{
    cc_ir_func irfunc;
    cc_ir_func_create(&irfunc, CC_STR("my_func"));

    cc_ir_block* entry = irfunc.entry_block;
    cc_ir_block* loop = cc_ir_func_insert(&irfunc, entry, CC_STR("loop"));
    cc_ir_block* end = cc_ir_func_insert(&irfunc, loop, CC_STR("end"));

    const int INT_SIZE = 4;

    cc_ir_block_iconst(entry, INT_SIZE, 10);
    cc_ir_block_iconst(entry, INT_SIZE, 9);
    cc_ir_block_sub(entry, INT_SIZE);

    cc_ir_block_iconst(loop, INT_SIZE, -1);
    cc_ir_block_add(loop, INT_SIZE);
    cc_ir_block_la(loop, loop->localid);
    cc_ir_block_jnz(loop, INT_SIZE);

    cc_ir_block_ret(end);

    printf("IR:\n");
    print_ir_func(&irfunc);

    cc_vm vm;
    cc_vm_create(&vm, 0x1000);
    
    vm.ip_func = &irfunc;
    vm.ip_block = irfunc.entry_block;

    do {
        cc_vm_step(&vm);
    } while (vm.vmexception == CC_VMEXCEPTION_NONE);

    printf("VM stack offset: 0x%X\n", (unsigned int)(vm.sp - vm.stack));
    printf("VM exception: %d\n", (int)vm.vmexception);

    cc_vm_destroy(&vm);

    cc_ir_func_destroy(&irfunc);
    return 1;
}