#include <cc/lib.h>
#include <cc/vm.h>
#include <cc/bigint.h>
#include <string.h>
#include <malloc.h>

void cc_vm_create(cc_vm* vm, size_t stack_size, const cc_vmprogram* program)
{
    memset(vm, 0, sizeof(*vm));
    vm->vmprogram = program;
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
    if (!vm->ip)
    {
        vm->vmexception = CC_VMEXCEPTION_INVALID_IP;
        return;
    }
    
    if (vm->sp < vm->stack || vm->sp > vm->stack + vm->stack_size)
    {
        vm->vmexception = CC_VMEXCEPTION_INVALID_SP;
        return;
    }

    // Decode instruction
    const cc_ir_ins* ins = (const cc_ir_ins*)vm->ip;
    // Increment IP
    vm->ip += sizeof(*ins);
    
    // Execute...
    switch (ins->opcode)
    {
    case CC_IR_OPCODE_ARGP:
    {
        void** dst = (void**)cc__vm_push(vm, sizeof(void*));
        if (!dst)
            return;
        *dst = vm->args_pointer;
        break;
    }
    // Special VM format: u32 is the frame pointer offset
    case CC_IR_OPCODE_ADDRL:
    {
        // Push local's address
        void** ptr_on_stack = (void*)cc__vm_push(vm, sizeof(*ptr_on_stack));
        if (!ptr_on_stack)
            return;

        *ptr_on_stack = vm->frame_pointer + ins->operand.u32;
        break;
    }
    // This is replaced with UCONST at compile-time
    //case CC_IR_OPCODE_SIZEL:
    //    break;
    // Special VM format: u32 is the frame pointer offset, data_size is the size
    case CC_IR_OPCODE_LOADL:
    {
        const void* src = vm->frame_pointer + ins->operand.u32;
        void* dst = cc__vm_push(vm, ins->data_size);
        if (!dst)
            return;
        memcpy(dst, src, ins->data_size);
        break;
    }
    case CC_IR_OPCODE_ADDRG:
    {
        if (ins->operand.symbolid >= vm->vmprogram->num_symbols)
        {
            vm->vmexception = CC_VMEXCEPTION_INVALID_SYMBOLID;
            return;
        }

        void* address_of_symbol = vm->vmprogram->symbols[ins->operand.symbolid].ptr;
        void** ptr_on_stack = (void**)cc__vm_push(vm, sizeof(*ptr_on_stack));
        if (!ptr_on_stack)
            return;
        *ptr_on_stack = address_of_symbol;
        break;
    }
    case CC_IR_OPCODE_SIZEP:
    {
        void* dst = cc__vm_push(vm, ins->data_size);
        if (!dst)
            return;
        cc_bigint_u32(ins->data_size, dst, sizeof(void*));
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
    case CC_IR_OPCODE_LOAD:
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
    case CC_IR_OPCODE_STORE:
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
    case CC_IR_OPCODE_DUPE:
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

    // TODO: Implement
    //case CC_IR_OPCODE_JMP:
    //{
    //    break;
    //}
    // Special VM format: blockid is the signed byte-offset to the block
    case CC_IR_OPCODE_JZ:
    case CC_IR_OPCODE_JNZ:
    {
        // Check condition
        bool condition = true;
        if (ins->opcode == CC_IR_OPCODE_JZ || ins->opcode == CC_IR_OPCODE_JNZ)
        {
            uint8_t* popped = cc__vm_pop(vm, ins->data_size);
            if (!popped)
                return;

            bool is_zero = true;
            for (cc_ir_datasize i = 0; i < ins->data_size; ++i)
            {
                if (popped[i])
                {
                    is_zero = false;
                    break;
                }
            }
            
            condition = is_zero == (ins->opcode == CC_IR_OPCODE_JZ);
        }
        
        if (condition)
            vm->ip += (int16_t)ins->operand.blockid;
        break;
    }
    case CC_IR_OPCODE_CALL:
    {
        uint8_t** target_on_stack = (uint8_t**)cc__vm_pop(vm, sizeof(void*));
        if (!target_on_stack)
            return;
        
        uint8_t* target = *target_on_stack;
        vm->args_pointer = vm->sp;

        uint8_t** old_ip;
        uint8_t** old_fp;
        uint8_t** old_ap;
        
        if (   !(old_ip = (uint8_t**)cc__vm_push(vm, sizeof(*old_ip)))
            || !(old_fp = (uint8_t**)cc__vm_push(vm, sizeof(*old_fp)))
            || !(old_ap = (uint8_t**)cc__vm_push(vm, sizeof(*old_ap)))
        ) {
            return;
        }

        *old_ip = vm->ip;
        *old_fp = vm->frame_pointer;
        *old_ap = vm->args_pointer;
        vm->ip = target;
        break;
    }
    case CC_IR_OPCODE_RET:
    {
        uint8_t** old_ip;
        uint8_t** old_fp;
        uint8_t** old_ap;

        if (   !(old_ap = (uint8_t**)cc__vm_pop(vm, sizeof(*old_ap)))
            || !(old_fp = (uint8_t**)cc__vm_pop(vm, sizeof(*old_fp)))
            || !(old_ip = (uint8_t**)cc__vm_pop(vm, sizeof(*old_ip)))
        ) {
            return;
        }
        
        vm->ip = *old_ip;
        vm->frame_pointer = *old_fp;
        vm->args_pointer = *old_ap;
        break;
    }
    case CC_IR_OPCODE_INT:
        vm->vmexception = CC_VMEXCEPTION_INTERRUPT;
        vm->interrupt = ins->operand.u32;
        break;
    case CC_IR_OPCODE_FRAME:
    {
        uint8_t* new_fp = cc__vm_push(vm, ins->operand.u32);
        if (!new_fp)
            return;
        vm->frame_pointer = new_fp;
        break;
    }
    default:
        vm->vmexception = CC_VMEXCEPTION_INVALID_CODE;
    }
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

size_t cc__vm_local_stack_offset(const cc_ir_func* func, cc_ir_localid localid)
{
    size_t offset = 0;
    for (size_t i = 0; i < func->num_locals; ++i)
    {
        const cc_ir_local* local = &func->locals[i];
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
    switch (local->typeid)
    {
    case CC_IR_TYPEID_INT:
    case CC_IR_TYPEID_FLOAT:
    case CC_IR_TYPEID_DATA:
        return local->data_size;
    case CC_IR_TYPEID_PTR:  return sizeof(void*);
    default:                return 0;
    }
}

void cc_vmprogram_create(cc_vmprogram* program) {
    memset(program, 0, sizeof(*program));
}
void cc_vmprogram_destroy(cc_vmprogram* program)
{
    for (size_t i = 0; i < program->num_ins_chunks; ++i)
        free(program->ins_chunks[i]);
    free(program->ins_chunks);
    free(program->ins_chunk_lengths);
    for (size_t i = 0; i < program->num_global_chunks; ++i)
        free(program->global_chunks[i]);
    free(program->global_chunks);
    for (size_t i = 0; i < program->num_symbols; ++i)
        cc_vmsymbol_destroy(&program->symbols[i]);
    free(program->symbols);
    cc_vmimport* import = program->first_import;
    do
    {
        cc_vmimport* next_import = import->next_import;
        cc_vmimport_destroy(import);
        import = next_import;
    } while (import);
}
cc_vmsymbol* cc_vmprogram_get_symbol(const cc_vmprogram* program, const char* name, size_t name_len)
{
    if (name == NULL)
        name_len = 0;
    if (name_len == (size_t)-1)
        name_len = strlen(name);
    if (!name_len)
        return NULL;

    for (size_t i = 0; i < program->num_symbols; ++i)
    {
        cc_vmsymbol* symbol = &program->symbols[i];
        if (symbol->name_len == name_len && !memcmp(symbol->name, name, name_len));
            return symbol;
    }
    return NULL;
}
bool cc_vmprogram_link(cc_vmprogram* program, const cc_ir_object* obj)
{
    cc_vmobject vmobj;
    size_t first_symbol_index = program->num_symbols;
    cc_vmobject_create(&vmobj);
    if (!cc_vmobject_compile(&vmobj, obj, first_symbol_index))
    {
        cc_vmobject_destroy(&vmobj);
        return false;
    }
    
    ++program->num_ins_chunks;
    ++program->num_global_chunks;
    program->num_symbols += vmobj.num_symbols;
    
    cc_vec_resize(program->ins_chunk_lengths,   program->num_ins_chunks);
    cc_vec_resize(program->ins_chunks,          program->num_ins_chunks);
    cc_vec_resize(program->global_chunks,       program->num_global_chunks);
    cc_vec_resize(program->symbols,             program->num_symbols);

    // Move data out of vmobject and into vmprogram, without unecessary copying

    program->ins_chunk_lengths[program->num_ins_chunks - 1] = vmobj.num_ins;
    program->ins_chunks[program->num_ins_chunks - 1] = vmobj.ins;
    program->global_chunks[program->num_global_chunks - 1] = vmobj.global_data;

    vmobj.ins = NULL;
    vmobj.global_data = NULL;

    cc_vmimport** last_import = &program->first_import;
    while (*last_import)
        last_import = &(*last_import)->next_import;
    *last_import = vmobj.first_import;
    vmobj.first_import = NULL;

    for (size_t i = 0; i < vmobj.num_symbols; ++i)
    {
        cc_vmsymbol* new_symbol = &program->symbols[first_symbol_index + i];
        cc_vmsymbol_move(new_symbol, &vmobj.symbols[i]);
        // All symbols are functions for now.
        // Because the code array is reallocating, the ptr is just an offset.
        // So convert the ptr back to an actual ptr
        new_symbol->ptr = (uint8_t*)program->ins_chunks[program->num_ins_chunks - 1] + (size_t)new_symbol->ptr;
    }

    // All data was moved. Now we may destroy our compiled object.
    cc_vmobject_destroy(&vmobj);
    
    // Try to resolve all imports

    cc_vmimport* import = program->first_import;
    cc_vmimport** prev_link = &program->first_import;
    while (import)
    {
        cc_vmimport* next_import = import->next_import;
        if (cc__vmprogram_resolve(program, import))
        {
            // Remove this import. It is resolved.
            cc_vmimport_destroy(import);
            *prev_link = next_import;
            import = next_import;
            continue;
        }

        prev_link = &import->next_import;
        import = next_import;
    }

    return true;
}

bool cc__vmprogram_resolve(cc_vmprogram* program, const cc_vmimport* import)
{
    cc_ir_symbolid symbolid = (cc_ir_symbolid)-1;
    
    // Find a corresponding symbol
    for (size_t i = 0; i < program->num_symbols; ++i)
    {
        const cc_vmsymbol* next_symbol = &program->symbols[i];
        if (next_symbol->name_len == import->name_len && !memcmp(next_symbol->name, import->name, next_symbol->name_len))
        {
            symbolid = (cc_ir_symbolid)i;
            break;
        }
    }
    if (symbolid == (cc_ir_symbolid)-1)
        return false; // No symbol found. Skip.

    // Symbol was found. Change all references in code
    for (size_t i = 0; i < import->num_code_refs; ++i)
        *import->code_refs[i] = symbolid;
    return true;
}

void cc_vmobject_create(cc_vmobject* vmobj) {
    memset(vmobj, 0, sizeof(*vmobj));
}
void cc_vmobject_destroy(cc_vmobject* vmobj)
{
    free(vmobj->ins);
    for (size_t i = 0; i < vmobj->num_symbols; ++i)
        cc_vmsymbol_destroy(&vmobj->symbols[i]);
    free(vmobj->symbols);

    cc_vmimport* import = vmobj->first_import;
    while (import)
    {
        cc_vmimport* next_import = import->next_import;
        cc_vmimport_destroy(import);
        import = next_import;
    }
}
bool cc_vmobject_compile(cc_vmobject* vmobject, const cc_ir_object* irobject, size_t first_symbol_index)
{
    vmobject->first_symbol_index = first_symbol_index;
    
    struct _symbolmap
    {
        cc_ir_symbolid ir_id; // ID in IR
        cc_ir_symbolid vm_id; // `vmobject->symbol` array-index + first_symbol_index
    };
    struct _importmap
    {
        cc_ir_symbolid ir_id; // ID in IR
        cc_vmimport* vmimport; // Pointer into vmobject import list
    };
    bool result = false;
    struct _symbolmap* symbolmap = NULL;
    struct _importmap* importmap = NULL;
    size_t num_imports = 0;

    // Copy all symbols and code into the obj
    for (size_t i = 0; i < irobject->num_symbols; ++i)
    {
        const cc_ir_symbol* irsymbol = &irobject->symbols[i];

        // Map external irsymbol to vmimport
        if (irsymbol->symbol_flags & CC_IR_SYMBOLFLAG_EXTERNAL)
        {
            cc_vmimport* vmimport = cc_vmimport_create(irsymbol->name, irsymbol->name_len);
            // Puts vmimport at front of the list
            vmimport->next_import = vmobject->first_import;
            vmobject->first_import = vmimport;

            ++num_imports;
            struct _importmap* entry = (struct _importmap*)cc_vec_resize(importmap, num_imports);
            entry->ir_id = irsymbol->symbolid;
            entry->vmimport = vmimport;
        }
        else // Map internal irsymbol to vmsymbol
        {
            ++vmobject->num_symbols;
            cc_vmsymbol* vmsymbol = (cc_vmsymbol*)cc_vec_resize(vmobject->symbols,  vmobject->num_symbols);
            struct _symbolmap* entry = (struct _symbolmap*)cc_vec_resize(symbolmap, vmobject->num_symbols);

            cc_vmsymbol_create(vmsymbol, irsymbol->name, irsymbol->name_len);
            entry->ir_id = irsymbol->symbolid;
            entry->vm_id = vmobject->num_symbols - 1 + first_symbol_index;

            // Symbol is a function. Append its code.
            {
                const cc_ir_func* irfunc = irsymbol->ptr.func;
                size_t code_offset = vmobject->num_ins * sizeof(vmobject->ins[0]);
                if (!cc__vmobject_flatten(vmobject, irfunc))
                {
                    result = false;
                    goto end;
                }
                
                vmsymbol->ptr = (void*)code_offset;
            }
        }
    }

    // Code transformations:
    // - Replace all internal symbol IDs with the new (array-friendly) IDs
    // - Store references to external symbols
    for (size_t i = 0; i < vmobject->num_ins; ++i)
    {
        cc_ir_ins* ins = &vmobject->ins[i];
        const cc_ir_ins_format* fmt = &cc_ir_ins_formats[ins->opcode];
        for (size_t i = 0; i < CC_IR_MAX_OPERANDS; ++i)
        {
            if (fmt->operand[i] != CC_IR_OPERAND_LOCAL)
                continue;
            
            cc_ir_localid* localid = &ins->operand.local;

            // Find localid in `importmap`, if it's an import
            cc_vmimport* vmimport = NULL;
            for (size_t i = 0; i < num_imports; ++i)
            {
                struct _importmap* entry = &importmap[i];
                if (entry->ir_id == *localid)
                {
                    vmimport = entry->vmimport;
                    break;
                }
            }

            // ID is an imported symbol. Add to the list of referencing IDs and continue
            if (vmimport)
            {
                ++vmimport->num_code_refs;
                cc_ir_localid** ref = (cc_ir_localid**)cc_vec_resize(vmimport->code_refs, vmimport->num_code_refs);
                *ref = localid;
                continue;
            }
            
            // Else, ID must be an internal symbol. Change it to the new ID.
            bool exists = false;
            for (size_t i = 0; i < vmobject->num_symbols; ++i)
            {
                struct _symbolmap* entry = &symbolmap[i];
                if (entry->ir_id == *localid)
                {
                    exists = true;
                    *localid = entry->vm_id;
                    break;
                }
            }

            // no such ID exists
            if (!exists)
            {
                result = false;
                goto end;
            }
        }
    }

    result = true;

end:
    free(symbolmap);
    free(importmap);
    return result;
}
bool cc__vmobject_flatten(cc_vmobject* vmobject, const cc_ir_func* func)
{
    struct _blockmap
    {
        cc_ir_blockid blockid; // this block's id
        size_t ins_index; // index in the object's instruction array
    };
    bool result = false;
    struct _blockmap* blockmap = NULL;
    size_t num_blocks = 0;

    if (func->entry_block == NULL)
    {
        result = true;
        goto end;
    }
    
    // Add the FRAME instruction to setup the stack frame (if any locals are present)
    size_t local_frame_size = cc__vm_local_stack_size(func);
    if (local_frame_size)
    {
        ++vmobject->num_ins;
        cc_ir_ins* ins = (cc_ir_ins*)cc_vec_resize(vmobject->ins, vmobject->num_ins);
        memset(ins, 0, sizeof(*ins));
        ins->opcode = CC_IR_OPCODE_FRAME;
        ins->operand.u32 = local_frame_size;
    }
    
    // Append the block's data
    for (const cc_ir_block* block = func->entry_block; block; block = block->next_block)
    {
        // Append to block map
        ++num_blocks;
        struct _blockmap* entry = (struct _blockmap*)cc_vec_resize(blockmap, num_blocks);
        entry->blockid = block->blockid;
        entry->ins_index = vmobject->num_ins;
        
        // Append instructions to vmobject
        size_t dst_index = vmobject->num_ins;
        vmobject->num_ins += block->num_ins;
        cc_vec_resize(vmobject->ins, vmobject->num_ins);
        memcpy(vmobject->ins + dst_index, block->ins, block->num_ins * sizeof(block->ins[0]));
    }

    // Code transformations:
    // - Replace locals logic with frame pointer logic
    // - Replace blockid operands with a byte offset to that block, relative to the next instruction
    for (size_t i = blockmap[0].ins_index; i < vmobject->num_ins; ++i)
    {   
        cc_ir_ins* ins = &vmobject->ins[i];

        // Replace locals logic with frame pointer logic
        switch (ins->opcode)
        {
        case CC_IR_OPCODE_ADDRL: // Set u32 stack offset
            ins->operand.u32 = (uint32_t)cc__vm_local_stack_offset(func, ins->operand.local);
            break;
        case CC_IR_OPCODE_SIZEL: // Replace with the uconst instruction
        {
            const cc_ir_local* irlocal = cc_ir_func_getlocal(func, ins->operand.local);
            if (irlocal == NULL) // No such local exists
            {
                result = false;
                goto end;
            }
            // (data_size is already set)
            ins->opcode = CC_IR_OPCODE_UCONST;
            ins->operand.u32 = (uint32_t)cc__vm_local_size(irlocal);
            break;
        }
        case CC_IR_OPCODE_LOADL: // Set u32 stack offset; Set data_size to local's size
        {
            const cc_ir_local* irlocal = cc_ir_func_getlocal(func, ins->operand.local);
            if (irlocal == NULL) // No such local exists
            {
                result = false;
                goto end;
            }
            ins->data_size = (cc_ir_datasize)cc__vm_local_size(irlocal);
            ins->operand.u32 = cc__vm_local_stack_offset(func, irlocal->localid);
        }
        }
        
        // Replace any blockid operands with byte offsets
        size_t next_ip = i + 1;
        const cc_ir_ins_format* fmt = &cc_ir_ins_formats[ins->opcode];
        for (size_t i = 0; i < CC_IR_MAX_OPERANDS; ++i)
        {
            if (fmt->operand[i] != CC_IR_OPERAND_BLOCKID)
                continue;

            cc_ir_blockid* blockid = &ins->operand.blockid;

            // Find relevant block in blockmap, and replace `blockid` with an offset
            const struct _blockmap* mapped_block = NULL;
            for (size_t i = 0; i < num_blocks; ++i)
            {
                if (blockmap[i].blockid == *blockid)
                {
                    mapped_block = &blockmap[i];
                    break;
                }
            }
            if (mapped_block == NULL) // No such block exists
            {
                result = false;
                goto end; 
            }
            
            *blockid = (cc_ir_blockid)((mapped_block->ins_index - next_ip) * sizeof(vmobject->ins[0]));
        }
    }

    result = true;

end:
    free(blockmap);
    return result;
}

void cc_vmsymbol_create(cc_vmsymbol* vmsymbol, const char* name, size_t name_len)
{
    memset(vmsymbol, 0, sizeof(*vmsymbol));
    vmsymbol->name = cc_strclone_char(name, name_len, &vmsymbol->name_len);
}
void cc_vmsymbol_destroy(cc_vmsymbol* vmsymbol) {
    free(vmsymbol->name);
}
void cc_vmsymbol_move(cc_vmsymbol* dst, cc_vmsymbol* src)
{
    memcpy(dst, src, sizeof(*dst));
    memset(src, 0, sizeof(*src));
}

cc_vmimport* cc_vmimport_create(const char* name, size_t name_len)
{
    cc_vmimport* vmimport = (cc_vmimport*)calloc(1, sizeof(*vmimport));
    memset(vmimport, 0, sizeof(vmimport));
    vmimport->name = cc_strclone_char(name, name_len, &vmimport->name_len);
    return vmimport;
}
void cc_vmimport_destroy(cc_vmimport* vmimport)
{
    free(vmimport->name);
    free(vmimport->code_refs);
    free(vmimport);
}