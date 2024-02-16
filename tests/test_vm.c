#include "test.h"
#include <stdio.h>
#include <stdbool.h>
#include <cc/vm.h>
#include <cc/ir.h>
#include <cc/bigint.h>

#define INT_SIZE 8
// Pop an int off the stack and checks it against the answer. Throws assertion errors.
#define INTERRUPT_CHECK_ANSWER 111
#define INTERRUPT_EXIT 222
#define INTERRUPT_PUTCHAR 333
#define FUNCTION_CHECK_ANSWER "check_answer"
#define FUNCTION_PRINT_INT "print_int"

static const int32_t TEST_ANSWER = 158;
static bool was_answer_found = false;
static bool was_exit_reached = false;
static char virtual_print_buffer[64];
static size_t virtual_print_cursor = 0;

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

    int call_depth = 0;
    while (vm.vmexception == CC_VMEXCEPTION_NONE && !was_exit_reached)
    {
        const cc_ir_ins* ins = (const cc_ir_ins*)vm.ip;

        // Get call depth
        if (ins->opcode == CC_IR_OPCODE_CALL)
            ++call_depth;
        else if (ins->opcode == CC_IR_OPCODE_RET)
            --call_depth;
        
        // Print instruction with indentation based on call depth
        for (int i = 0; i < call_depth; ++i)
            printf("  ");
        print_ir_ins(ins, NULL);
        putchar('\n');

        // Execute instruction
        cc_vm_step(&vm);
        
        // Handle interrupts
        if (vm.vmexception == CC_VMEXCEPTION_INTERRUPT)
        {
            vm.vmexception = CC_VMEXCEPTION_NONE;
            interrupt_handler(&vm, vm.interrupt);
        }

        // Print stack data with indentation based on call depth
        for (int i = 0; i < call_depth; ++i)
            printf("  ");
        printf("  0x%X : %%llX = %llX\n", (unsigned int)(vm.sp - vm.stack), *(long long*)vm.sp);
    }

    printf("VM stack offset: 0x%X\n", (unsigned int)(vm.sp - vm.stack));
    printf("VM exception: %d\n", (int)vm.vmexception);
    printf("VM console output: %.*s\n", (int)virtual_print_cursor, virtual_print_buffer);
    
    test_assert("The VM must reach the exit with no exceptions", vm.vmexception == CC_VMEXCEPTION_NONE);    

    cc_vm_destroy(&vm);

    test_assert("Expected the answer to be found", was_answer_found);
    test_assert("Expected VM to print something", virtual_print_cursor > 0);

    int32_t printed_int;
    cc_bigint_atoi(sizeof(printed_int), &printed_int, 10, virtual_print_buffer, virtual_print_cursor);
    test_assert("Expected the answer to be printed correctly", printed_int == TEST_ANSWER);
    return 1;
}

static cc_ir_object* create_main_object()
{
    cc_ir_object* obj = (cc_ir_object*)calloc(1, sizeof(*obj));
    cc_ir_object_create(obj);

    cc_ir_func* func = cc_ir_object_add_func(obj, "main", -1);
    cc_ir_symbolid check_answer = cc_ir_object_import(obj, false, FUNCTION_CHECK_ANSWER, -1);
    cc_ir_symbolid print_int = cc_ir_object_import(obj, false, FUNCTION_PRINT_INT, -1);

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
    cc_ir_block_loadl(end, x);                       //   print_int(x)
    cc_ir_block_addrg(end, print_int);
    cc_ir_block_call(end);
    cc_ir_block_free(end, INT_SIZE);
    cc_ir_block_loadl(end, x);                       //   check_answer(x)
    cc_ir_block_addrg(end, check_answer);            //
    cc_ir_block_call(end);                           //
    cc_ir_block_free(end, INT_SIZE);                 //   ; Caller must cleanup the stack...
    cc_ir_block_int(end, INTERRUPT_EXIT);            //   return
    cc_ir_block_ret(end);                            //   return

    return obj;
}
static cc_ir_object* create_library_object()
{
    cc_ir_object* obj = (cc_ir_object*)calloc(1, sizeof(*obj));
    cc_ir_object_create(obj);
    cc_ir_symbolid print_int_recursive;

    // void check_answer(int i)
    {
        cc_ir_func* func = cc_ir_object_add_func(obj, FUNCTION_CHECK_ANSWER, -1);
        cc_ir_block* block = func->entry_block;

        cc_ir_block_argp(block);
        cc_ir_block_load(block, INT_SIZE);
        cc_ir_block_int(block, INTERRUPT_CHECK_ANSWER);
        cc_ir_block_ret(block);
    }
    // void print_int_recursive(int i)
    {
        /*
        Pseudocode:
            void print_int_recursive(int i)
            {
                if (i == 0)
                    return;
                print_int_recursive(i / 10);
                putchar('0' + (i % 10));
            }
        */
        cc_ir_func* func = cc_ir_object_add_func(obj, "print_int_recursive", -1);
        print_int_recursive = func->symbolid;
        cc_ir_block* entry = func->entry_block;
        cc_ir_block* logic = cc_ir_func_insert(func, entry, "logic", -1);
        cc_ir_block* end = cc_ir_func_insert(func, logic, "end", -1);
        
        cc_ir_block_argp(entry); // Load i
        cc_ir_block_load(entry, INT_SIZE);
        cc_ir_block_jz(entry, INT_SIZE, end); // if (i == 0) goto end
        
        cc_ir_block_uconst(logic, INT_SIZE, 10);        // Load 10
        cc_ir_block_argp(logic);                        // Load i
        cc_ir_block_load(logic, INT_SIZE);
        cc_ir_block_udiv(logic, INT_SIZE);              // Calc i / 10
        cc_ir_block_addrg(logic, print_int_recursive);  // Load self
        cc_ir_block_call(logic);                        // Call self(i / 10)
        cc_ir_block_free(logic, INT_SIZE);
        cc_ir_block_uconst(logic, INT_SIZE, 10);        // Load 10
        cc_ir_block_argp(logic);                        // Load i
        cc_ir_block_load(logic, INT_SIZE);
        cc_ir_block_umod(logic, INT_SIZE);              // Calc (char)(i % 10)
        cc_ir_block_zext(logic, INT_SIZE, 1);
        cc_ir_block_uconst(logic, 1, '0');              // Load (char)'0'
        cc_ir_block_add(logic, 1);                      // Calc '0' + (i % 10)
        cc_ir_block_int(logic, INTERRUPT_PUTCHAR);      // putchar
        
        cc_ir_block_ret(end);
    }
    // void print_int(int i)
    {
        /*
        Pseudocode:
            void print_int(i)
            {
                if (i == 0)
                    putchar('0');
                else
                    print_int_recursive(i);
            }
        */
        
        cc_ir_func* func = cc_ir_object_add_func(obj, FUNCTION_PRINT_INT, -1);
        cc_ir_block* if_stmt = func->entry_block;
        cc_ir_block* putchar = cc_ir_func_insert(func, if_stmt, "putchar", -1);
        cc_ir_block* recurse = cc_ir_func_insert(func, putchar, "recurse", -1);
        cc_ir_block* end = cc_ir_func_insert(func, recurse, "end", -1);
        
        cc_ir_block_argp(if_stmt);                      // Load i
        cc_ir_block_load(if_stmt, INT_SIZE);
        cc_ir_block_jnz(if_stmt, INT_SIZE, recurse);    // if (i != 0) goto recurse
        
        cc_ir_block_uconst(putchar, 1, '0');              // Load '0'
        cc_ir_block_int(putchar, INTERRUPT_PUTCHAR);    // putchar

        cc_ir_block_argp(recurse);                      // Load i
        cc_ir_block_load(recurse, INT_SIZE);
        cc_ir_block_addrg(recurse, print_int_recursive);// Call print_int_recursive(i)
        cc_ir_block_call(recurse);
        cc_ir_block_free(recurse, INT_SIZE);            // Pop args

        cc_ir_block_ret(end);

    }
    
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
        break;
    case INTERRUPT_PUTCHAR:
    {
        char* char_on_stack = cc__vm_pop(vm, 1);
        test_assert("Expected a char on the stack", char_on_stack != NULL);
        if (virtual_print_cursor >= sizeof(virtual_print_buffer))
            break;
        virtual_print_buffer[virtual_print_cursor] = *char_on_stack;
        ++virtual_print_cursor;
        break;
    }
    }
}