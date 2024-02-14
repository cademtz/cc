#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "ir.h"

/**
 * @file
 * @brief A virtual machine based on the IR
 * 
 * Slight transformation is required to make the IR interpreter-friendly:
 * - Block IDs are replaced with a byte-offset to that block, relative to the next instruction
 * - Symbol IDs are converted to indexes into a globals array
 * - Locals logic is replaced with frame-pointer logic
 * 
 * Use @ref cc_vmprogram_create and @ref cc_vmprogram_link to create an interpretable program.
 */

typedef struct cc_vmprogram cc_vmprogram;

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
    /// @brief Use of a symbol id which does not exist in the current function 
    CC_VMEXCEPTION_INVALID_SYMBOLID,
    /// @brief The next instruction could not be interpreted
    CC_VMEXCEPTION_INVALID_CODE,
} cc_vmexception;

/**
 * @brief The virtual machine state.
 * 
 * The instruction pointer is a combination of the `ip`, `ip_block`, and `ip_func` members.
 */
typedef struct cc_vm
{
    const cc_vmprogram* vmprogram;
    /// @brief The last exception. Must be cleared before resuming execution.
    cc_vmexception vmexception;
    /// @brief A user-defined interrupt code. Assigned when `vmexception == CC_VMEXCEPTION_INTERRUPT`.
    uint32_t interrupt;

    uint8_t* stack;
    size_t stack_size;

    /// @brief A resizing scratch buffer for bigint operations
    uint8_t* scratch;
    size_t scratch_size;
    
    uint8_t* ip;
    uint8_t* sp;
    /// @brief Pointer to the locals stack (which is inside the regular stack)
    uint8_t* frame_pointer;
    /// @brief Pointer to the arguments stack (which is inside the regular stack)
    uint8_t* args_pointer;
} cc_vm;

/// @brief A symbol that points to its corresponding global data
typedef struct cc_vmsymbol
{
    char* name;
    size_t name_len;
    /// @brief Pointer to the symbol's corresponding data
    void* ptr;
} cc_vmsymbol;

/// @brief Unresolved code references to a symbol
typedef struct cc_vmimport
{
    char* name;
    size_t name_len;
    /// @brief Pointers to the source of every referencing @ref cc_ir_symbolid in code
    cc_ir_symbolid** code_refs;
    size_t num_code_refs;
    struct cc_vmimport* next_import;
} cc_vmimport;

/// @brief A single IR object compiled for the VM
typedef struct cc_vmobject
{
    /// @brief Every instruction in one array
    cc_ir_ins* ins;
    size_t num_ins;
    uint8_t* global_data;
    /// @brief Size of global data (in bytes)
    size_t size_global_data;
    /// @brief A lookup table for internal symbols. Index into it with `symbolid - first_symbol_index`
    cc_vmsymbol* symbols;
    size_t num_symbols;
    /// @brief All code references to the symbols array are offset by this value
    size_t first_symbol_index;
    cc_vmimport* first_import;
} cc_vmobject;

/**
 * @brief A program with specially-compiled IR for the VM.
 * 
 * - All instructions are compiled into one array
 * - Local jumps use a byte offset to the desired instruction
 * - Local IDs are replaced with stack frame offsets
 * - Globals are compiled and stored in one array
 */
typedef struct cc_vmprogram
{
    /// @brief Array of chunk pointers. Each chunk contains all instructions for a linked object
    cc_ir_ins** ins_chunks;
    /// @brief The number of instructions in a chunk by index.
    size_t* ins_chunk_lengths;
    size_t num_ins_chunks;
    /// @brief Array of chunk pointers. Each chunk contains all globals for a linked object
    uint8_t** global_chunks;
    size_t num_global_chunks;
    /// @brief A lookup table for the globals. Array index corresponds with `symbolid`.
    cc_vmsymbol* symbols;
    size_t num_symbols;
    cc_vmimport* first_import;
} cc_vmprogram;

void cc_vm_create(cc_vm* vm, size_t stack_size, const cc_vmprogram* program);
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
size_t cc__vm_local_stack_offset(const cc_ir_func* func, cc_ir_localid localid);
/// @brief The total size of the locals stack for a function
size_t cc__vm_local_stack_size(const cc_ir_func* func);
/// @brief The size of a local, in bytes
/// @return 0 if the local is not a type with a size (such as blocks)
size_t cc__vm_local_size(const cc_ir_local* local);
/// @brief Set IP to the function and assign registers before executing a function.
/// @details Nothing is stored or restored. That is the job of call/return.
/// @return `false` if vmexception was written
bool cc__vm_enter_func(cc_vm* vm, const cc_ir_func* func);
/// @brief Free the function's locals before it returns
/// @details Nothing is stored or restored. That is the job of call/return.
/// @return `false` if vmexception was written
bool cc__vm_leave_func(cc_vm* vm, const cc_ir_func* func);

void cc_vmprogram_create(cc_vmprogram* program);
void cc_vmprogram_destroy(cc_vmprogram* program);
cc_vmsymbol* cc_vmprogram_get_symbol(const cc_vmprogram* program, const char* name, size_t name_len);
bool cc_vmprogram_link(cc_vmprogram* program, const cc_ir_object* obj);
bool cc__vmprogram_resolve(cc_vmprogram* program, const cc_vmimport* import);
void cc_vmobject_create(cc_vmobject* vmobj);
void cc_vmobject_destroy(cc_vmobject* vmobj);
bool cc_vmobject_compile(cc_vmobject* vmobject, const cc_ir_object* irobject, size_t first_symbol_index);
/// @brief Flatten `func` into one array of instructions and append to `vmobject`
/// @details Every blockid is replaced with a byte offset to that block (relative to the next instruction)
bool cc__vmobject_flatten(cc_vmobject* vmobject, const cc_ir_func* func);
void cc_vmsymbol_create(cc_vmsymbol* vmsymbol, const char* name, size_t name_len);
void cc_vmsymbol_destroy(cc_vmsymbol* vmsymbol);
void cc_vmsymbol_move(cc_vmsymbol* dst, cc_vmsymbol* src);
cc_vmimport* cc_vmimport_create(const char* name, size_t name_len);
void cc_vmimport_destroy(cc_vmimport* vmimport);