#include <cc/vm.h>
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
        
        if (ins->data_size > sizeof(ins->operand.u32))
        {
            write_ptr += ins->data_size - sizeof(ins->operand.u32);

            int clear_byte = 0;
            if (ins->opcode == CC_IR_OPCODE_ICONST && (int32_t)ins->operand.u32 < 0)
                clear_byte = 0xFF;
            memset(vm->sp, clear_byte, ins->data_size);
        }
        
        cc__vm_write_int(vm->sp, ins->operand.u32, ins->data_size);
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

    case CC_IR_OPCODE_ADD:
    case CC_IR_OPCODE_SUB:
    {
        if (ins->data_size == sizeof(uint32_t))
        {
            uint32_t* lhs = (uint32_t*)cc__vm_pop(vm, sizeof(*lhs));
            uint32_t* rhs = (uint32_t*)cc__vm_pop(vm, sizeof(*rhs));
            uint32_t* dst = (uint32_t*)cc__vm_push(vm, sizeof(*dst));
            if (!lhs || !rhs || !dst)
                return;
            
            switch (ins->opcode)
            {
            case CC_IR_OPCODE_ADD: *dst = *lhs + *rhs; break;
            case CC_IR_OPCODE_SUB: *dst = *lhs - *rhs; break;
            }
        }
        break;
    }

    case CC_IR_OPCODE_JMP:
    case CC_IR_OPCODE_JNZ:
    {
        // Pop jump location from stack.
        // In this case, the VM can only jump to blocks, so the location points to a block struct
        void** ptr_on_stack = (void**)cc__vm_pop(vm, sizeof(void*));
        if (!ptr_on_stack)
            return;
        const cc_ir_block* dst_block = (const cc_ir_block*)*ptr_on_stack;

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