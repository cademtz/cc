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
 * 
 */
typedef struct cc_vm
{
    uint8_t* stack;
    size_t stack_size;
    
    uint8_t* sp;
    size_t ip;
    const cc_ir_block* ip_block;
    const cc_ir_func* ip_func;

    /// @brief The last exception. Must be cleared before resuming execution.
    cc_vmexception vmexception;
    /// @brief A user-defined interrupt code. Assigned when `vmexception == CC_VMEXCEPTION_INTERRUPT`.
    uint8_t interrupt;
} cc_vm;

void cc_vm_create(cc_vm* vm, size_t stack_size);
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
/// @brief Write an integer
/// @param num_bytes Size of the integer, no larger than sizeof u32
void cc__vm_write_int(void* dst, uint32_t u32, size_t num_bytes);
/// @brief Read an integer
void cc__vm_read_int(uint32_t* dst, const void* src, size_t num_bytes);