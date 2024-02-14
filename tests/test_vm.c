#include "test.h"
#include <stdio.h>
#include <stdbool.h>
#include <cc/vm.h>
#include <cc/ir.h>
#include <cc/bigint.h>

#define INT_SIZE 64
// Pop an int off the stack and checks it against the answer. Throws assertion errors.
#define INTERRUPT_CHECK_ANSWER 0xBEEF
#define INTERRUPT_EXIT 0xBA11
#define FUNCTION_CHECK_ANSWER "check_answer"

static const int32_t TEST_ANSWER = 158;
static bool was_answer_found = false;
static bool was_exit_reached = false;

cc_ir_object* create_main_object();
cc_ir_object* create_library_object();
void interrupt_handler(cc_vm* vm, uint32_t interrupt);

int test_vm(void)
{
    cc_vmprogram program;
    cc_vmprogram_create(&program);

    {
        cc_ir_object* obj_main = create_main_object();
        cc_ir_object* obj_library = create_library_object();
        test_assert("main object must link successfully", cc_vmprogram_link(&program, obj_main));
        test_assert("library object must link successfully", cc_vmprogram_link(&program, obj_library));
        cc_ir_object_destroy(obj_main);
        cc_ir_object_destroy(obj_library);
    }

    const cc_vmsymbol* symbol_main = cc_vmprogram_get_symbol(&program, "main", -1);
    test_assert("A symbol named 'main' must be exposed in the program", symbol_main != NULL);

    cc_vm vm;
    cc_vm_create(&vm, 0x1000, &program);
    vm.ip = (uint8_t*)symbol_main->ptr;

    while (vm.vmexception == CC_VMEXCEPTION_NONE && !was_exit_reached)
    {
        print_ir_ins((const cc_ir_ins*)vm.ip, NULL);
        printf("\n");
        cc_vm_step(&vm);
        printf("  0x%X : %%llX = %llX\n", (unsigned int)(vm.sp - vm.stack), *(long long*)vm.sp);

        if (vm.vmexception == CC_VMEXCEPTION_INTERRUPT)
        {
            vm.vmexception = CC_VMEXCEPTION_NONE;
            interrupt_handler(&vm, vm.interrupt);
        }
    }

    printf("VM stack offset: 0x%X\n", (unsigned int)(vm.sp - vm.stack));
    printf("VM exception: %d\n", (int)vm.vmexception);
    
    test_assert("The VM must reach the exit with no exceptions", vm.vmexception == CC_VMEXCEPTION_NONE);    

    cc_vm_destroy(&vm);

    test_assert("Expected the answer to be found", was_answer_found);
    return 1;
}

static cc_ir_object* create_main_object()
{
    cc_ir_object* obj = (cc_ir_object*)calloc(1, sizeof(*obj));
    cc_ir_object_create(obj);

    cc_ir_func* func = cc_ir_object_add_func(obj, "main", -1);
    cc_ir_symbolid check_answer = cc_ir_object_import(obj, false, FUNCTION_CHECK_ANSWER, -1);

    cc_ir_block* entry = func->entry_block;
    cc_ir_block* loop = cc_ir_func_insert(func, entry, "loop", -1);
    cc_ir_block* end = cc_ir_func_insert(func, loop, "end", -1);
    cc_ir_localid x = cc_ir_func_int(func, INT_SIZE, "x");

    // A program that calculates the answer in an overcomplicated way:
                                                     // entry:
    cc_ir_block_uconst(entry, INT_SIZE, 9);          //   x = 9
    cc_ir_block_addrl(entry, x);                     //
    cc_ir_block_store(entry, INT_SIZE);              //
                                                     // loop:
    cc_ir_block_uconst(loop, INT_SIZE, 1);           //   x -= 1
    cc_ir_block_loadl(loop, x);                      //
    cc_ir_block_sub(loop, INT_SIZE);                 //
    cc_ir_block_addrl(loop, x);                      //
    cc_ir_block_store(loop, INT_SIZE);               //
    cc_ir_block_uconst(loop, INT_SIZE, 9);           //   x *= 9
    cc_ir_block_loadl(loop, x);                      //
    cc_ir_block_umul(loop, INT_SIZE);                //
    cc_ir_block_addrl(loop, x);                      //
    cc_ir_block_store(loop, INT_SIZE);               //
    cc_ir_block_loadl(loop, x);                      //   x = 10000 / x
    cc_ir_block_uconst(loop, INT_SIZE, 10000);       //
    cc_ir_block_udiv(loop, INT_SIZE);                //
    cc_ir_block_addrl(loop, x);                      //
    cc_ir_block_store(loop, INT_SIZE);               //
    cc_ir_block_loadl(loop, x);                      //   if (x != TEST_ANSWER) then goto loop
    cc_ir_block_uconst(loop, INT_SIZE, TEST_ANSWER); //
    cc_ir_block_sub(loop, INT_SIZE);                 //
    cc_ir_block_jnz(loop, INT_SIZE, loop);           //
                                                     // end:
    cc_ir_block_loadl(end, x);                       //   check_answer(x)
    cc_ir_block_addrg(end, check_answer);            //
    cc_ir_block_call(end);                           //
    cc_ir_block_addrl(end, x);                       //   ; Caller must cleanup the stack...
    cc_ir_block_store(end, INT_SIZE);                //   ; There is no stackfree ins yet, so we'll just store x back into x.
    cc_ir_block_int(end, INTERRUPT_EXIT);            //   return
    cc_ir_block_ret(end);                            //   return

    return obj;
}
static cc_ir_object* create_library_object()
{
    cc_ir_object* obj = (cc_ir_object*)calloc(1, sizeof(*obj));
    cc_ir_object_create(obj);

    cc_ir_func* func = cc_ir_object_add_func(obj, FUNCTION_CHECK_ANSWER, -1);

    cc_ir_block* block = func->entry_block;
    cc_ir_block_argp(block);
    cc_ir_block_load(block, INT_SIZE);
    cc_ir_block_int(block, INTERRUPT_CHECK_ANSWER);
    cc_ir_block_ret(block);
    
    return obj;
}

static void interrupt_handler(cc_vm* vm, uint32_t code)
{
    switch (code)
    {
    case INTERRUPT_CHECK_ANSWER:
        uint8_t answer_extended[INT_SIZE];
        cc_bigint_i32(INT_SIZE, answer_extended, TEST_ANSWER);
        
        void* result = cc__vm_pop(vm, INT_SIZE);
        test_assert("Expected an integer on the stack", result != NULL);
        test_assert("Expected the correct answer", !memcmp(result, answer_extended, INT_SIZE));
        was_answer_found = true;
        break;
    case INTERRUPT_EXIT:
        printf("VM has reached the exit interrupt\n");
        was_exit_reached = true;
    }
}