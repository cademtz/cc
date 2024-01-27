#pragma once
#include <stdint.h>

/**
 * @file
 * @brief Data structures and constants for the IR.
 * 
 * An IR function is composed of blocks, and blocks are composed of instructions.
 * A block may only change control flow using its last instruction.
 * This means jumps, interrupts, and returns may not appear in the middle of a block.
 * 
 * IR functions have locals ( @ref cc_ir_local ). Each local has a size and address.
 * Examples of things that are locals:
 * - An integer (of any size)
 * - A large constant
 * - One of the arguments
 * - One of the code blocks
 * - The function itself
 * 
 * All locals are referenced by id ( @ref cc_ir_localid ).
 * A local's size and address can be read with a special load instruction.
 * 
 * Integer overflow must always wrap.
 */

#define CC_IR_MAX_OPERANDS 2
typedef uint16_t cc_ir_localid;
typedef uint32_t cc_ir_datasize;

/// @brief Enum of every opcode
enum cc_ir_opcode
{
    // === Loading and storing locals ===

    /// @brief Push a local's address
    CC_IR_OPCODE_LA,
    /// @brief Push a local's size
    CC_IR_OPCODE_LS,
    /// @brief Push a local's value
    CC_IR_OPCODE_LLD,
    /// @brief Store value in local
    CC_IR_OPCODE_LSTO,

    // === Loading and storing ===

    /// @brief Push a sign-extended 32-bit integer
    /// @details Pseudocode: `push(i)`
    CC_IR_OPCODE_ICONST,
    /// @brief Push a zero-extended 32-bit integer
    /// @details Pseudocode: `push(i)`
    CC_IR_OPCODE_UCONST,
    /// @brief Load value at address
    /// @details Pseudocode: `push(*pop())`
    CC_IR_OPCODE_LD,
    /// @brief Store value at address
    /// @details Pseudocode: `*pop() = pop()`
    CC_IR_OPCODE_STO,
    
    // === Arithmetic ===

    /// @brief Add integers
    /// @details Pseudocode: `push(pop() + pop())`
    CC_IR_OPCODE_ADD,
    /// @brief Subtract integers
    /// @details Pseudocode: `push(pop() - pop())`
    CC_IR_OPCODE_SUB,

    // === Control flow ===

    /// @brief Call address
    /// @details: Pseudocode: `call pop()`
    CC_IR_OPCODE_CALL,
    /// @brief Jump to address
    /// @details Pseudocode: `goto pop()`
    CC_IR_OPCODE_JMP,
    /// @brief Jump to address if value is not zero
    /// @details Pseudocode: `if (pop() != 0) goto pop()`
    CC_IR_OPCODE_JNZ,
    /// @brief Return
    /// @details Pseudocode: `return`
    CC_IR_OPCODE_RET,

    /// @brief The number of valid opcodes.
    /// Do not create any opcodes greater-than or equal-to this value.
    CC_IR_OPCODE__COUNT,
};

/// @brief Enum of every operand type
enum cc_ir_operand
{
    /// @brief No operand
    CC_IR_OPERAND_NONE,
    /// @brief Local variable
    CC_IR_OPERAND_LOCAL,
    /// @brief Data size
    CC_IR_OPERAND_DATASIZE,
    /// @brief A raw 32-bit constant
    CC_IR_OPERAND_U32,
};

enum cc_ir_localtypeid
{
    CC_IR_LOCALTYPEID_BLOCK,
    CC_IR_LOCALTYPEID_FUNC,
    CC_IR_LOCALTYPEID_INT,
    CC_IR_LOCALTYPEID_PTR,
};

/// @brief The format of an instruction
typedef struct cc_ir_ins_format
{
    const char* mnemonic;
    /// @brief A value from @ref cc_ir_operand
    uint8_t operand[CC_IR_MAX_OPERANDS];
} cc_ir_ins_format;

/// @brief A function's local variable
typedef struct cc_ir_local
{
    /**
     * @brief (optional) Name. May be `NULL`.
     * 
     * Pointer must be freed.
     */
    char* name;
    /** 
     * @brief Size in bytes. Currently, only integers use this.
     * 
     * A pointer size is only known while compiling the IR.
     * A function or block's size is only known after partial or full compilation of the IR. 
     */
    cc_ir_datasize data_size;
    /// @brief A value from @ref cc_ir_localtypeid
    uint16_t localtypeid;
    /// @brief This local's ID
    cc_ir_localid localid;
} cc_ir_local;

/**
 * @brief An intermediate instruction.
 */
typedef struct cc_ir_ins
{
    /// @brief A value from @ref cc_ir_opcode
    uint8_t opcode;
    /// @brief Operation size (in bytes)
    cc_ir_datasize data_size;
    union
    {
        /// @brief local
        cc_ir_localid local;
        /// @brief 32-bit constant
        uint32_t u32;
    } operand;
} cc_ir_ins;

/**
 * @brief A block of instructions ( @ref cc_ir_ins ), part of a linked list of blocks.
 * 
 * Only the last instruction in a block can change the control flow.
 * Otherwise, execution falls-through to @ref cc_ir_block.next_block.
 */
typedef struct cc_ir_block
{
    /// @brief A reallocating array of instructions
    cc_ir_ins* ins;
    size_t num_ins;
    /// @brief The next block in the list. May be `NULL`.
    struct cc_ir_block* next_block;
    /// @brief The local describing this block
    cc_ir_localid localid;
} cc_ir_block;

/**
 * @brief A function with code blocks and locals.
 * 
 * Every function starts with one block (the entry block) and one local (the function).
 * A local can be a variable, a block, a large constant, or the function itself.
 */
typedef struct cc_ir_func
{
    /// @brief The entrypoint for execution
    cc_ir_block* entry_block;
    /// @brief A reallocating array of locals
    cc_ir_local* locals;
    size_t num_blocks;
    size_t num_locals;
    cc_ir_localid _next_localid;
} cc_ir_func;

/// @brief Array of every IR instruction's format, ordered by opcode
const extern cc_ir_ins_format cc_ir_ins_formats[CC_IR_OPCODE__COUNT];

/// @brief Create a new IR function with a new empty entry block
void cc_ir_func_create(cc_ir_func* func, const char* name);
/// @brief Create a clone of `func`
/// @param func The original function
/// @param clone An uninitialized struct to store the cloned function
void cc_ir_func_clone(const cc_ir_func* func, cc_ir_func* clone);
/// @brief Free the IR function, its blocks, and its locals
void cc_ir_func_destroy(cc_ir_func* func);
/// @brief Get the function's localid
static cc_ir_localid cc_ir_func_getlocalself(const cc_ir_func* func) {
    return 0;
}
/// @brief Get information about a function's local
cc_ir_local* cc_ir_func_getlocal(const cc_ir_func* func, cc_ir_localid localid);
/// @brief Get a block in the function
/// @return nullptr if the local is not a block.
cc_ir_block* cc_ir_func_getblock(const cc_ir_func* func, cc_ir_localid localid);
/**
 * @brief Create a new block following a previous one
 * @param prev Previous block
 * @param name (optional) Block name
 * @return A new block
 */
cc_ir_block* cc_ir_func_insert(cc_ir_func* func, cc_ir_block* prev, const char* name);
/**
 * @brief Create a new int-local
 * @param size_bytes Integer size, in bytes
 * @param name (optional) Variable name
 * @return The new local id
 */
cc_ir_localid cc_ir_func_int(cc_ir_func* func, uint32_t size_bytes, const char* name);
/**
 * Create a new pointer-local
 * @param name (optional) Variable name
 * @return The new local id
 */
cc_ir_localid cc_ir_func_ptr(cc_ir_func* func, const char* name);
/**
 * @brief Create a new local with the same type as the original
 * @param localid The local to clone
 * @param name (optional) Variable name
 * @return The new local id
 */
cc_ir_localid cc_ir_func_clonelocal(cc_ir_func* func, cc_ir_localid localid, const char* name);

/**
 * @brief Insert or append a new instruction anywhere in the block
 * @param index An index, where `index <= block->num_ins`
 * @param ins The instruction
 */
void cc_ir_block_insert(cc_ir_block* block, size_t index, const cc_ir_ins* ins);
/// @brief Append an instruction to the block
static inline void cc_ir_block_append(cc_ir_block* block, const cc_ir_ins* ins) {
    cc_ir_block_insert(block, block->num_ins, ins);
}

void cc_ir_block_la(cc_ir_block* block, cc_ir_localid localid);
void cc_ir_block_ls(cc_ir_block* block, cc_ir_localid localid);
void cc_ir_block_lld(cc_ir_block* block, cc_ir_localid localid);
void cc_ir_block_lsto(cc_ir_block* block, cc_ir_localid localid);
void cc_ir_block_iconst(cc_ir_block* block, cc_ir_datasize data_size, int32_t value);
void cc_ir_block_uconst(cc_ir_block* block, cc_ir_datasize data_size, uint32_t value);
void cc_ir_block_ld(cc_ir_block* block, cc_ir_datasize data_size);
void cc_ir_block_sto(cc_ir_block* block, cc_ir_datasize data_size);
void cc_ir_block_add(cc_ir_block* block, cc_ir_datasize data_size);
void cc_ir_block_sub(cc_ir_block* block, cc_ir_datasize data_size);
void cc_ir_block_call(cc_ir_block* block);
void cc_ir_block_jmp(cc_ir_block* block);
void cc_ir_block_jnz(cc_ir_block* block);
void cc_ir_block_ret(cc_ir_block* block);
