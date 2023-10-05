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
 * IR functions have locals. Each local has a size and address. A local can be:
 * - A code block
 * - An integer (of any size)
 * - A large constant
 * - The function itself
 * 
 * All locals are referenced by id. ( @ref cc_ir_localid )
 * A local's size and address can be read with a special load instruction.
 */

#define CC_IR_MAX_OPERANDS 3
typedef uint8_t cc_ir_localid;

/// @brief Enum of every opcode.
/// If you modify this enum, then update @ref cc_ir_ins_formats to match.
enum cc_ir_opcode
{
    /// @brief Load constant `value` in `dst`.
    /// Format: ldc local dst, u32 value
    CC_IR_OPCODE_LDC,
    /// @brief Load the `target` local's address in `dst`.
    /// Format: ldla local dst, local target
    CC_IR_OPCODE_LDLA,
    /// @brief Load the `target` local's size in `dst`.
    /// Format: ldls local dst, local target
    CC_IR_OPCODE_LDLS,
    
    /// @brief Store `lhs + rhs` in `dst`.
    /// Format: add local dst, local lhs, local rhs
    CC_IR_OPCODE_ADD,
    /// @brief Store `lhs - rhs` in `dst`.
    /// Format: add local dst, local lhs, local rhs
    CC_IR_OPCODE_SUB,

    /// @brief Jump to `addr` if `value` is not zero.
    /// Format: jnz local addr, local value
    CC_IR_OPCODE_JNZ,
    /// @brief Return
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
    /// @brief A raw 32-bit constant
    CC_IR_OPERAND_U32,
};

enum cc_ir_localtypeid
{
    CC_IR_LOCALTYPEID_BLOCK,
    CC_IR_LOCALTYPEID_FUNC,
    CC_IR_LOCALTYPEID_INT,
};

/// @brief The format of an instruction
typedef struct cc_ir_ins_format
{
    const char* mnemonic;
    /// @brief A value from @ref cc_ir_operand
    uint8_t operand[CC_IR_MAX_OPERANDS];
} cc_ir_ins_format;

/// @brief Description of a local, including its name, type, and size
typedef struct cc_ir_local
{
    /// @brief (optional) Name
    const char* name;
    /// @brief Variable size in bytes. Zero for other types.
    uint32_t var_size;
    /// @brief A value from @ref cc_ir_localtypeid
    uint16_t localtypeid;
    /// @brief This local's ID
    cc_ir_localid localid;
} cc_ir_local;

/// @brief An intermediate instruction.
/// Composed of an opcode, 1 write-only operand, and 2 read-only operands.
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

/// @brief An array of continuous instructions.
/// Control flow only changes at the end of the block.
typedef struct cc_ir_block
{
    cc_ir_ins* ins;
    size_t num_ins;
    /// @brief The next block to visit. May be `nullptr`.
    /// The next block is part of the same function.
    struct cc_ir_block* next_block;
} cc_ir_block;

/// @brief A series of blocks and locals.
/// Currently, removing locals is not supported.
typedef struct cc_ir_func
{
    cc_ir_block* blocks;
    cc_ir_local* locals;
    size_t num_blocks;
    size_t num_locals;
    cc_ir_localid _next_localid;
} cc_ir_func;

/// @brief Array of every IR instruction's format, ordered by opcode
const extern cc_ir_ins_format cc_ir_ins_formats[CC_IR_OPCODE__COUNT];

void cc_ir_func_create(cc_ir_func* func);
void cc_ir_func_destroy(cc_ir_func* func);
/// @brief Create a new block in the function
/// @param name (optional) Block label
cc_ir_block* cc_ir_func_block(cc_ir_func* func, const char* name);
/// @brief Create a new int local in the function
/// @param size_bytes Integer size, in bytes
/// @param name (optional) Variable name
/// @return The new local id
cc_ir_localid cc_ir_func_int(cc_ir_func* func, uint32_t size_bytes, const char* name);

/// @brief Load a constant integer to a local
/// @return Instruction index
size_t cc_ir_block_ldc(cc_ir_block* block, cc_ir_localid dst, uint32_t value);
/// @brief Add two locals
/// @return Instruction index
size_t cc_ir_block_add(cc_ir_block* block, cc_ir_localid dst, cc_ir_localid lhs, cc_ir_localid rhs);
/// @brief Subtract two locals
/// @return Instruction index
size_t cc_ir_block_sub(cc_ir_block* block, cc_ir_localid dst, cc_ir_localid lhs, cc_ir_localid rhs);
