#include <cc/vm.h>
#include <cc/bigint.h>
#include <string.h>
#include <malloc.h>

void cc_vm_create(cc_vm* vm, size_t stack_size, const cc_ir_func* entrypoint)
{
    memset(vm, 0, sizeof(*vm));
    vm->stack_size = stack_size;
    vm->stack = (uint8_t*)malloc(stack_size);
    vm->sp = vm->stack + vm->stack_size;
    cc__vm_enter_func(vm, entrypoint);
}

void cc_vm_destroy(cc_vm* vm)
{
    free(vm->stack);
    free(vm->scratch);
    memset(vm, 0, sizeof(vm));
}

void cc_vm_step(cc_vm* vm)
{
    if (vm->sp < vm->stack || vm->sp > vm->stack + vm->stack_size)
    {
        vm->vmexception = CC_VMEXCEPTION_INVALID_SP;
        return;
    }

    const cc_ir_ins* ins = cc_vm_next_ins(vm);
    if (!ins)
    {
        vm->vmexception = CC_VMEXCEPTION_INVALID_IP;
        return;
    }
    
    // Increment the IP
    ++vm->ip;
    if (vm->ip >= vm->ip_block->num_ins)
    {
        // Go to the next block, skipping any empty ones
        while (vm->ip_block->next_block)
        {
            vm->ip_block = vm->ip_block->next_block;
            vm->ip = 0;
            if (vm->ip < vm->ip_block->num_ins)
                break; // We found a non-empty block
        }    
    }
    
    // Execute...
    switch (ins->opcode)
    {
    case CC_IR_OPCODE_LA:
    {
        size_t offset = cc__vm_local_stack_offset(vm, ins->operand.local);
        if (offset == (size_t)-1)
        {
            vm->vmexception = CC_VMEXCEPTION_INVALID_LOCALID;
            return;
        }
        
        // Push local's address
        void** ptr_on_stack = (void*)cc__vm_push(vm, sizeof(void*));
        if (!ptr_on_stack)
            return;

        *ptr_on_stack = vm->locals_sp + offset;
        break;
    }
    case CC_IR_OPCODE_LS:
    {
        const cc_ir_local* local = cc_ir_func_getlocal(vm->ip_func, ins->operand.local);
        size_t local_size = 0;
        if (local)
            local_size = cc__vm_local_size(local);
        
        if (!local_size)
        {
            vm->vmexception = CC_VMEXCEPTION_INVALID_LOCALID;
            return;
        }
        
        void* value_on_stack = cc__vm_push(vm, ins->data_size);
        if (!value_on_stack)
            return;

        cc_bigint_u32(ins->data_size, value_on_stack, (uint32_t)local_size);
        break;
    }
    case CC_IR_OPCODE_LLD:
    {
        const cc_ir_local* local = cc_ir_func_getlocal(vm->ip_func, ins->operand.local);
        size_t local_size = 0;
        size_t local_offset;
        if (local)
            local_size = cc__vm_local_size(local), local_offset = cc__vm_local_stack_offset(vm, local->localid);
        
        if (!local_size)
        {
            vm->vmexception = CC_VMEXCEPTION_INVALID_LOCALID;
            return;
        }

        const void* src = vm->locals_sp + local_offset;
        void* dst = cc__vm_push(vm, local_size);
        if (!dst)
            return;
            
        memcpy(dst, src, local_size);
        break;
    }
    case CC_IR_OPCODE_ICONST:
    case CC_IR_OPCODE_UCONST:
    {   
        uint8_t* write_ptr = cc__vm_push(vm, ins->data_size);
        if (!write_ptr)
            return;

        if (ins->opcode == CC_IR_OPCODE_ICONST)
            cc_bigint_i32(ins->data_size, write_ptr, (int32_t)ins->operand.u32);
        else
            cc_bigint_u32(ins->data_size, write_ptr, ins->operand.u32);
        break;
    }
    case CC_IR_OPCODE_LD:
    {
        // Pop the source pointer
        void** src_ptr_on_stack = (void**)cc__vm_pop(vm, sizeof(void*));
        if (!src_ptr_on_stack)
            return;
        const void* src_ptr = *src_ptr_on_stack;

        // Push the source data onto the stack
        void* dst_ptr = cc__vm_push(vm, ins->data_size);
        if (!dst_ptr)
            return;
        
        memcpy(dst_ptr, src_ptr, ins->data_size);
        break;
    }
    case CC_IR_OPCODE_STO:
    {
        // Pop the destination pointer
        void** dst_ptr_on_stack = (void**)cc__vm_pop(vm, sizeof(void*));
        if (!dst_ptr_on_stack)
            return;
        void* dst_ptr = *dst_ptr_on_stack;

        // Pop value
        const void* src_ptr = cc__vm_pop(vm, ins->data_size);
        if (!src_ptr)
            return;

        // Store the popped data at the destination
        memcpy(dst_ptr, src_ptr, ins->data_size);
        break;
    }
    case CC_IR_OPCODE_DUP:
    {
        const void* src = cc__vm_pop(vm, ins->data_size);
        if (!src)
            return;
            
        cc__vm_push(vm, ins->data_size);
        void* dst = cc__vm_push(vm, ins->data_size);
        if (!dst)
            return;

        memcpy(dst, src, ins->data_size);
        break;
    }

    // === Unary operations ===

    case CC_IR_OPCODE_NEG:
    case CC_IR_OPCODE_NOT:
    case CC_IR_OPCODE_ZEXT:
    case CC_IR_OPCODE_SEXT:
    {
        uint32_t* lhs = (uint32_t*)cc__vm_pop(vm, ins->data_size);
        if (!lhs)
            return;

        size_t push_size = ins->data_size;

        switch (ins->opcode)
        {
        case CC_IR_OPCODE_NEG: cc_bigint_neg(ins->data_size, lhs); break;
        case CC_IR_OPCODE_NOT: cc_bigint_not(ins->data_size, lhs); break;
        case CC_IR_OPCODE_ZEXT:
        case CC_IR_OPCODE_SEXT:
            push_size = ins->operand.extend_data_size;
            if (ins->opcode == CC_IR_OPCODE_ZEXT)
                cc_bigint_extend_zero(ins->operand.extend_data_size, lhs, ins->data_size, lhs);
            else
                cc_bigint_extend_sign(ins->operand.extend_data_size, lhs, ins->data_size, lhs);
            break;
        }

        cc__vm_push(vm, push_size);
    }


    // === Binary operations ===

    case CC_IR_OPCODE_ADD:
    case CC_IR_OPCODE_SUB:
    case CC_IR_OPCODE_UMUL:
    case CC_IR_OPCODE_UDIV:
    case CC_IR_OPCODE_UMOD:
    case CC_IR_OPCODE_AND:
    case CC_IR_OPCODE_OR:
    case CC_IR_OPCODE_XOR:
    case CC_IR_OPCODE_LSH:
    case CC_IR_OPCODE_RSH:
    {
        uint32_t* lhs = (uint32_t*)cc__vm_pop(vm, ins->data_size);
        uint32_t* rhs = (uint32_t*)cc__vm_pop(vm, ins->data_size);
        if (!lhs || !rhs)
            return;

        const void* result_ptr = lhs;
        
        switch (ins->opcode)
        {
        case CC_IR_OPCODE_ADD: cc_bigint_add(ins->data_size, lhs, rhs); break;
        case CC_IR_OPCODE_SUB: cc_bigint_sub(ins->data_size, lhs, rhs); break;
        case CC_IR_OPCODE_MUL: cc_bigint_mul(ins->data_size, lhs, rhs); break;
        case CC_IR_OPCODE_UMUL: cc_bigint_umul(ins->data_size, lhs, rhs); break;
        case CC_IR_OPCODE_DIV:
        case CC_IR_OPCODE_UDIV:
        case CC_IR_OPCODE_MOD:
        case CC_IR_OPCODE_UMOD:
        {
            uint8_t _stack_quotient[8], _stack_remainder[8];
            void* quotient = _stack_quotient, * remainder = _stack_remainder;

            // Allocate scratch space for the quotient and remainder if required
            if (ins->data_size > sizeof(_stack_quotient))
            {
                vm->scratch = (uint8_t*)realloc(vm->scratch, ins->data_size * 2);
                quotient = vm->scratch;
                remainder = vm->scratch + ins->data_size;
            }

            // Signed operation
            if (ins->opcode == CC_IR_OPCODE_DIV || ins->opcode == CC_IR_OPCODE_MOD)
                cc_bigint_div(ins->data_size, lhs, rhs, quotient, remainder);
            else // Unsigned operation
                cc_bigint_udiv(ins->data_size, lhs, rhs, quotient, remainder);

            // Point the result to our quotient or remainder
            result_ptr = quotient;
            if (ins->opcode == CC_IR_OPCODE_UMOD)
                result_ptr = remainder;
            break;
        }

        case CC_IR_OPCODE_AND:  cc_bigint_and(ins->data_size, lhs, rhs); break;
        case CC_IR_OPCODE_OR:   cc_bigint_or(ins->data_size, lhs, rhs); break;
        case CC_IR_OPCODE_XOR:  cc_bigint_xor(ins->data_size, lhs, rhs); break;
        case CC_IR_OPCODE_LSH:  cc_bigint_lsh(ins->data_size, lhs, rhs); break;
        case CC_IR_OPCODE_RSH:  cc_bigint_rsh(ins->data_size, lhs, rhs); break;
        }

        void* dst = cc__vm_push(vm, ins->data_size);
        if (!dst)
            return;
        memcpy(dst, result_ptr, ins->data_size);
        break;
    }

    case CC_IR_OPCODE_JMP:
    case CC_IR_OPCODE_JNZ:
    {
        // Get the destination
        const cc_ir_block* dst_block = cc_ir_func_getblock(vm->ip_func, ins->operand.local);
        if (dst_block == NULL)
        {
            vm->vmexception = CC_VMEXCEPTION_INVALID_LOCALID;
            return;
        }

        // Check condition
        bool condition = true;
        if (ins->opcode == CC_IR_OPCODE_JNZ)
        {
            condition = false;
            
            uint8_t* popped = cc__vm_pop(vm, ins->data_size);
            if (!popped)
                return;

            // Fulfill the condition if any byte of the popped value is non-zero
            for (cc_ir_datasize i = 0; i < ins->data_size; ++i)
            {
                if (popped[i])
                {
                    condition = true;
                    break;
                }
            }
        }
        
        if (condition)
        {
            vm->ip = 0;
            vm->ip_block = dst_block;
        }
        break;
    }
    case CC_IR_OPCODE_CALL:
    case CC_IR_OPCODE_RET:
    {
        vm->ip = 0;
        vm->ip_block = NULL;
        vm->ip_func = NULL;
        break;
    }
    }
}

const cc_ir_ins* cc_vm_next_ins(const cc_vm* vm)
{
    if (!vm->ip_func || !vm->ip_block || vm->ip >= vm->ip_block->num_ins)
        return NULL;
    return &vm->ip_block->ins[vm->ip];
}

bool cc__vm_offset_stack(cc_vm* vm, int32_t offset)
{
    bool bad_sp = vm->sp < vm->stack || vm->sp > vm->stack + vm->stack_size;
    if (!bad_sp)
    {
        vm->sp += offset;
        bad_sp = vm->sp < vm->stack || vm->sp > vm->stack + vm->stack_size;
        if (!bad_sp)
            return true;
    }

    vm->vmexception = CC_VMEXCEPTION_INVALID_SP;
    return false;
}

uint8_t* cc__vm_push(cc_vm* vm, uint32_t num_bytes)
{
    if (!cc__vm_offset_stack(vm, -(int32_t)num_bytes))
        return NULL;
    return vm->sp;
}

uint8_t* cc__vm_pop(cc_vm* vm, uint32_t num_bytes)
{
    uint8_t* previous_sp = vm->sp;
    if (!cc__vm_offset_stack(vm, (int32_t)num_bytes))
        return NULL;
    return previous_sp;
}

size_t cc__vm_local_stack_offset(cc_vm* vm, cc_ir_localid localid)
{
    size_t offset = 0;
    for (size_t i = 0; i < vm->ip_func->num_locals; ++i)
    {
        const cc_ir_local* local = &vm->ip_func->locals[i];
        size_t local_size = cc__vm_local_size(local);
        if (local->localid == localid)
            return local_size ? offset : (size_t)-1;
        offset += local_size;
    }

    return (size_t)-1;
}

size_t cc__vm_local_stack_size(const cc_ir_func* func)
{
    size_t offset = 0;
    for (size_t i = 0; i < func->num_locals; ++i)
        offset += cc__vm_local_size(&func->locals[i]);
    return offset;
}

size_t cc__vm_local_size(const cc_ir_local* local)
{
    switch (local->localtypeid)
    {
    case CC_IR_LOCALTYPEID_INT: return local->data_size;
    case CC_IR_LOCALTYPEID_PTR: return sizeof(void*);
    default:                    return 0;
    }
}

bool cc__vm_enter_func(cc_vm* vm, const cc_ir_func* func)
{
    if (func == NULL)
    {
        vm->vmexception = CC_VMEXCEPTION_INVALID_IP;
        return false;
    }

    size_t local_stack_size = cc__vm_local_stack_size(func);
    uint8_t* local_stack = cc__vm_push(vm, local_stack_size);
    if (!local_stack)
        return false;
    
    vm->locals_sp = local_stack;
    vm->ip = 0;
    vm->ip_block = func->entry_block;
    vm->ip_func = func;
    return true;
}