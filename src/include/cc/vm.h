#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "ir.h"

typedef enum cc_vmexception
{
    CC_VMEXCEPTION_NONE,
    /// @brief An interrupt instruction was executed
    CC_VMEXCEPTION_INTERRUPT,
    /// @brief The IP is not in executable memory
    CC_VMEXCEPTION_INVALID_IP,
    /// @brief The SP is outside the allocated stack
    CC_VMEXCEPTION_INVALID_SP,
    /// @brief Use of a local id which does not exist in the current function 
    CC_VMEXCEPTION_INVALID_LOCALID,
} cc_vmexception;

/**
 * @brief The virtual machine state.
 * 
 * The instruction pointer is a combination of the `ip`, `ip_block`, and `ip_func` members.
 */
typedef struct cc_vm
{
    uint8_t* stack;
    size_t stack_size;

    /// @brief A resizing scratch buffer for bigint operations
    uint8_t* scratch;
    size_t scratch_size;
    
    uint8_t* sp;
    /// @brief Pointer to the locals stack (which is inside the regular stack)
    uint8_t* locals_sp;
    size_t ip;
    const cc_ir_block* ip_block;
    const cc_ir_func* ip_func;

    /// @brief The last exception. Must be cleared before resuming execution.
    cc_vmexception vmexception;
    /// @brief A user-defined interrupt code. Assigned when `vmexception == CC_VMEXCEPTION_INTERRUPT`.
    uint8_t interrupt;
} cc_vm;

void cc_vm_create(cc_vm* vm, size_t stack_size, const cc_ir_func* entrypoint);
void cc_vm_destroy(cc_vm* vm);
/// @brief Execute at the IP and increment the IP
void cc_vm_step(cc_vm* vm);
/// @brief Get the next instruction to be executed
/// @return `nullptr` if the IP is invalid 
const cc_ir_ins* cc_vm_next_ins(const cc_vm* vm);
/// @brief Move the SP
/// @return `false` if vmexception was written
bool cc__vm_offset_stack(cc_vm* vm, int32_t offset);
/// @brief Pop `num_bytes` from stack
/// @return Pointer to the popped bytes, or `nullptr` if vmexception was written
uint8_t* cc__vm_pop(cc_vm* vm, uint32_t num_bytes);
/// @brief Push `num_bytes` to stack
/// @return Pointer to the pushed bytes, or `nullptr` if vmexception was written
uint8_t* cc__vm_push(cc_vm* vm, uint32_t num_bytes);
/// @brief The byte offset to a local in the locals stack
/// @return `(size_t)-1` if the localid is invalid or not in locals stack (such as blocks)
size_t cc__vm_local_stack_offset(cc_vm* vm, cc_ir_localid localid);
/// @brief The total size of the locals stack for a function
size_t cc__vm_local_stack_size(const cc_ir_func* func);
/// @brief The size of a local, in bytes
/// @return 0 if the local is not a type with a size (such as blocks)
size_t cc__vm_local_size(const cc_ir_local* local);
/// @brief Set IP to the function and assign registers before executing a function.
/// @details Nothing is stored or restored. That is the job of call/return.
/// @return `false` if vmexception was written
bool cc__vm_enter_func(cc_vm* vm, const cc_ir_func* func);