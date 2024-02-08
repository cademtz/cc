#include <cc/vm.h>
#include <cc/bigint.h>
#include <string.h>
#include <malloc.h>

void cc_vm_create(cc_vm* vm, size_t stack_size)
{
    memset(vm, 0, sizeof(*vm));
    vm->stack_size = stack_size;
    vm->stack = (uint8_t*)malloc(stack_size);
    vm->sp = vm->stack + vm->stack_size;
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
        // Get local by ID
        const cc_ir_local* local = cc_ir_func_getlocal(vm->ip_func, ins->operand.local);
        if (!local)
        {
            vm->vmexception = CC_VMEXCEPTION_INVALID_LOCALID;
            return;
        }
        
        // Push local's address
        void** ptr_on_stack = (void*)cc__vm_push(vm, sizeof(void*));
        if (!ptr_on_stack)
            return;

        switch (local->localtypeid)
        {
        case CC_IR_LOCALTYPEID_FUNC: *ptr_on_stack = (void*)vm->ip_func; break;
        case CC_IR_LOCALTYPEID_BLOCK: *ptr_on_stack = cc_ir_func_getblock(vm->ip_func, local->localid); break;
        default: *ptr_on_stack = NULL;
        }
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
    {
        uint32_t* lhs = (uint32_t*)cc__vm_pop(vm, ins->data_size);
        if (!lhs)
            return;

        switch (ins->opcode)
        {
        case CC_IR_OPCODE_NEG: cc_bigint_neg(ins->data_size, lhs); break;
        case CC_IR_OPCODE_NOT: cc_bigint_not(ins->data_size, lhs); break;
        }

        cc__vm_push(vm, ins->data_size);
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
        case CC_IR_OPCODE_UMUL: cc_bigint_umul(ins->data_size, lhs, rhs); break;
        case CC_IR_OPCODE_UDIV:
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

void cc__vm_write_int(void* dst, uint32_t u32, size_t num_bytes)
{
    if (num_bytes > sizeof(u32))
        num_bytes = sizeof(u32);
    switch (num_bytes)
    {
    case 1: *(uint8_t*)dst = (uint8_t)u32; break;
    case 2: *(uint16_t*)dst = (uint16_t)u32; break;
    case 3:
        *(uint16_t*)dst = (uint16_t)u32;
        *((uint8_t*)dst + 3) = (uint8_t)(u32 >> 16);
        break;
    case 4: *(uint32_t*)dst = u32; break;
    }
}