#include "test.h"
#include <stdio.h>
#include <cc/vm.h>
#include <cc/ir.h>
#include <cc/bigint.h>

int test_vm(void)
{
    cc_ir_func irfunc;
    cc_ir_func_create(&irfunc, CC_STR("my_func"));

    cc_ir_block* entry = irfunc.entry_block;
    cc_ir_block* loop = cc_ir_func_insert(&irfunc, entry, CC_STR("loop"));
    cc_ir_block* end = cc_ir_func_insert(&irfunc, loop, CC_STR("end"));

    #define INT_SIZE 64

    const uint32_t answer = 158;

    // A simple program that leaves the number 14 on the stack after execution
                                                // entry:
    cc_ir_block_iconst(entry, INT_SIZE, 9);     //   x = 9
                                                // loop:
    cc_ir_block_iconst(loop, INT_SIZE, -1);     //   x = -1 + x
    cc_ir_block_add(loop, INT_SIZE);            //
    cc_ir_block_iconst(loop, INT_SIZE, 9);      //   x = 9 * x
    cc_ir_block_umul(loop, INT_SIZE);           //   
    cc_ir_block_iconst(loop, INT_SIZE, 10000);  //   x = 10000 / x
    cc_ir_block_udiv(loop, INT_SIZE);           //   
    cc_ir_block_dup(loop, INT_SIZE);            //   if (x != answer) then goto loop
    cc_ir_block_iconst(loop, INT_SIZE, answer); //
    cc_ir_block_sub(loop, INT_SIZE);            //
    cc_ir_block_jnz(loop, INT_SIZE, loop);      //
                                                // end:
    cc_ir_block_ret(end);                       //   return

    printf("IR:\n");
    print_ir_func(&irfunc);

    cc_vm vm;
    cc_vm_create(&vm, 0x1000);
    
    vm.ip_func = &irfunc;
    vm.ip_block = irfunc.entry_block;

    do {
        // End execution right before the return instruction is executed
        if (vm.ip_block == end && vm.ip == 0)
            break;
        cc_vm_step(&vm);
        printf("VM stack offset: 0x%X\n", (unsigned int)(vm.sp - vm.stack));
        printf("%%lld: %lld\n", *(long long*)vm.sp);
    } while (vm.vmexception == CC_VMEXCEPTION_NONE);

    printf("VM stack offset: 0x%X\n", (unsigned int)(vm.sp - vm.stack));
    printf("VM exception: %d\n", (int)vm.vmexception);
    
    test_assert("The VM must not throw an exception", vm.vmexception == CC_VMEXCEPTION_NONE);
    test_assert("The VM must be halted at the return instruction", vm.ip_block == end && vm.ip == 0);
    
    void* result = cc__vm_pop(&vm, INT_SIZE);
    uint8_t answer_extended[INT_SIZE];
    cc_bigint_i32(INT_SIZE, answer_extended, answer);

    test_assert("Expected one integer remaining on the stack", result != NULL);
    test_assert("Expected the correct answer on stack", !memcmp(result, answer_extended, INT_SIZE));

    cc_vm_destroy(&vm);

    cc_ir_func_destroy(&irfunc);
    return 1;
}