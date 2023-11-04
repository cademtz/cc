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
 * IR functions have locals ( @ref cc_ir_local ). Each local has a size and address. A local can be:
 * - A code block
 * - An integer (of any size)
 * - A large constant
 * - The function itself
 * 
 * All locals are referenced by id ( @ref cc_ir_localid ).
 * A local's size and address can be read with a special load instruction.
 * 
 * Integer overflow must always wrap.
 */

#define CC_IR_MAX_OPERANDS 3
typedef uint16_t cc_ir_localid;

/// @brief Enum of every opcode.
/// If you modify this enum, then update @ref cc_ir_ins_formats to match.
enum cc_ir_opcode
{
    /// @brief Load constant `value` in `dst` and clear any remaining bits.
    /// Format: `ldc local dst, u32 value`
    CC_IR_OPCODE_LDC,
    /// @brief Load the `target` local's address in `dst`.
    /// Format: `ldla local dst, local target`
    CC_IR_OPCODE_LDLA,
    /// @brief Load the `target` local's size in `dst`.
    /// Format: `ldls local dst, local target`
    CC_IR_OPCODE_LDLS,
    /// @brief Assign `dst` the value of `src`
    /// Format: `mov local dst, local src`
    CC_IR_OPCODE_MOV,
    
    /// @brief Store `lhs + rhs` in `dst`.
    /// Format: add local dst, local lhs, local rhs
    CC_IR_OPCODE_ADD,
    /// @brief Store `lhs - rhs` in `dst`.
    /// Format: `add local dst, local lhs, local rhs`
    CC_IR_OPCODE_SUB,

    /// @brief Jump to `block`.
    /// Format: `jmp local block`
    CC_IR_OPCODE_JMP,
    /// @brief Jump to `block` if `value` is not zero.
    /// Format: `jnz local block, local value`
    CC_IR_OPCODE_JNZ,
    /// @brief Return.
    /// Format: `ret`
    CC_IR_OPCODE_RET,
    /// @brief Return a local value.
    /// Format: `ret local value`
    CC_IR_OPCODE_RETL,

    /// @brief A hint that `dst` may be either `lhs` or `rhs`.
    /// Format: `phi local dst, local lhs, local rhs`
    CC_IR_OPCODE_PHI,

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
     * @brief Variable size in bytes. Currently, only integers use this.
     * 
     * A pointer size is only known while compiling the IR.
     * A function or block's size is only known after partial or full compilation of the IR. 
     */
    uint32_t var_size;
    /// @brief A value from @ref cc_ir_localtypeid
    uint16_t localtypeid;
    /// @brief This local's ID
    cc_ir_localid localid;
} cc_ir_local;

/** @brief An intermediate instruction.
 * 
 * Composed of an opcode, 1 write-only operand, and 2 read-only operands.
 * 
 * A read-only operand may contain an address that is written to,
 * but that operand remains unchanged.
 */
typedef struct cc_ir_ins
{
    /// @brief A value from @ref cc_ir_opcode
    uint8_t opcode;
    /// @brief Currently used as padding. Should be zero.
    uint8_t reserved;
    /// @brief Write-only operand.
    /// This local receives the result (if any) of the operation.
    cc_ir_localid write;
    /// @brief Read-only operands
    union
    {
        /// @brief Additional locals to read from
        cc_ir_localid local[2];
        /// @brief A raw 32-bit constant
        uint32_t u32;
    } read;
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
/// @brief Get the function's localid to itself
static cc_ir_localid cc_ir_func_getlocalself(const cc_ir_func* func) {
    return 0;
}
/// @brief Get information about a function's local
cc_ir_local* cc_ir_func_getlocal(const cc_ir_func* func, cc_ir_localid localid);
/// @brief Get a block in the function
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
 * @brief Insert a new instruction anywhere in the block's code
 * @param index An index, where `index <= block->num_ins`
 * @param ins The instruction
 */
void cc_ir_block_insert(cc_ir_block* block, size_t index, cc_ir_ins ins);
/// @brief Load an integer constant: `dst = value`
void cc_ir_block_ldc(cc_ir_block* block, cc_ir_localid dst, uint32_t value);
/// @brief Load a local's address: `dst = & src`
void cc_ir_block_ldla(cc_ir_block* block, cc_ir_localid dst, cc_ir_localid src);
/// @brief Load a local's size: `dst = sizeof(src)`
void cc_ir_block_ldls(cc_ir_block* block, cc_ir_localid dst, cc_ir_localid src);
/// @brief Add two locals: `dst = lhs + rhs`
void cc_ir_block_add(cc_ir_block* block, cc_ir_localid dst, cc_ir_localid lhs, cc_ir_localid rhs);
/// @brief Subtract two locals: `dst = lhs - rhs`
void cc_ir_block_sub(cc_ir_block* block, cc_ir_localid dst, cc_ir_localid lhs, cc_ir_localid rhs);
/// @brief Jump: `goto dst`
void cc_ir_block_jmp(cc_ir_block* block, const cc_ir_block* dst);
/// @brief Jump if not zero: `if (value != 0) goto dst`
void cc_ir_block_jnz(cc_ir_block* block, const cc_ir_block* dst, cc_ir_localid value);
/// @brief Return
void cc_ir_block_ret(cc_ir_block* block);
/// @brief Return a local value
void cc_ir_block_retl(cc_ir_block* block, cc_ir_localid value);
/// @brief Hint at the usage of two locals
void cc_ir_block_phi(cc_ir_block* block, cc_ir_localid dst, cc_ir_localid lhs, cc_ir_localid rhs);

/// @brief Copy a local's value to another: `dst = src`
cc_ir_ins cc_ir_ins_mov(cc_ir_localid dst, cc_ir_localid src);