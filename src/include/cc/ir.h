#pragma once
#include <stdint.h>
#include <stdbool.h>

/**
 * @file
 * @brief Data structures and constants for the IR.
 * 
 * Functions
 * ---------
 * An IR function is composed of code blocks and locals.
 * 
 * Blocks
 * ------
 * A block may only change control flow using its last instruction.
 * This means jumps, interrupts, and returns may not appear in the middle of a block.
 *
 * Locals
 * ------ 
 * IR functions have locals. Each local has a size and address.
 * Examples of things that are locals:
 * - An integer (of any size)
 * - The function itself
 * 
 * All locals are referenced by id ( @ref cc_ir_localid ).
 * A local's size and address can be read with special instructions ( @ref CC_IR_OPCODE_ADDRL and @ref CC_IR_OPCODE_SIZEL ).
 * 
 * 
 * Pointers and arithmetic
 * ----------
 * - Integer overflow will wrap
 * - Signed integer division and modulus will return positive values
 * - The size of a pointer depends on the host machine.
 * - Use opcode @ref CC_IR_OPCODE_SIZEP to load the size of a pointer.
 * - Use the size `0` in arithmetic instructions for pointer math.
 */

#define CC_IR_MAX_OPERANDS 2
typedef uint16_t cc_ir_localid;
typedef uint32_t cc_ir_symbolid;
typedef uint16_t cc_ir_blockid;
typedef uint16_t cc_ir_datasize;

/// @brief Enum of every opcode
enum cc_ir_opcode
{
    // === Registers, locals, and globals ===

    /// @brief Push the args pointer
    CC_IR_OPCODE_ARGP,
    /// @brief Address of local
    /// @details Pseudocode: `push(&local)`
    CC_IR_OPCODE_ADDRL,
    /// @brief Size of local
    /// @details Pseudocode: `push(sizeof(local))`
    CC_IR_OPCODE_SIZEL,
    /// @brief Load local
    /// @details Pseudocode: `push(local)`
    CC_IR_OPCODE_LOADL,
    /// @brief Address of global
    /// @details Pseudocode: `push(&global)`
    CC_IR_OPCODE_ADDRG,
    
    // === Constants and magic ===

    /// @brief Size of pointer
    /// @details Pseudocode: `push(sizeof(void*))`
    CC_IR_OPCODE_SIZEP,

    // === Loading and storing ===

    /// @brief Push a sign-extended 32-bit integer
    /// @details Pseudocode: `push(i)`
    CC_IR_OPCODE_ICONST,
    /// @brief Push a zero-extended 32-bit integer
    /// @details Pseudocode: `push(i)`
    CC_IR_OPCODE_UCONST,
    /// @brief Load value at address
    /// @details Pseudocode: `push(*pop())`
    CC_IR_OPCODE_LOAD,
    /// @brief Store value at address
    /// @details Pseudocode: `*pop() = pop()`
    CC_IR_OPCODE_STORE,
    /// @brief Duplicate the last n bytes on the stack
    CC_IR_OPCODE_DUPE,
    
    // === Arithmetic ===

    /// @brief Add integers
    /// @details Pseudocode: `push(pop() + pop())`
    CC_IR_OPCODE_ADD,
    /// @brief Subtract integers
    /// @details Pseudocode: `push(pop() - pop())`
    CC_IR_OPCODE_SUB,
    /// @brief Multiply signed ints
    /// @details Pseudocode: `push(pop() * pop())`
    CC_IR_OPCODE_MUL,
    /// @brief Multiply unsigned ints
    /// @details Pseudocode: `push(pop() * pop())`
    CC_IR_OPCODE_UMUL,
    /// @brief Divide signed ints
    /// @details Pseudocode: `push(pop() / pop())`
    CC_IR_OPCODE_DIV,
    /// @brief Divide unsigned ints
    /// @details Pseudocode: `push(pop() / pop())`
    CC_IR_OPCODE_UDIV,
    /// @brief Modulo signed ints
    /// @details Pseudocode: `push(pop() % pop())`
    CC_IR_OPCODE_MOD,
    /// @brief Modulo unsigned ints
    /// @details Pseudocode: `push(pop() % pop())`
    CC_IR_OPCODE_UMOD,
    /// @brief Negate a signed int
    /// @details Pseudocode: `push(-pop())`
    CC_IR_OPCODE_NEG,

    // === Bitwise operations ===

    /// @brief Bitwise not
    /// @details Pseudocode: `push(~pop())`
    CC_IR_OPCODE_NOT,
    /// @brief Bitwise and
    /// @details Pseudocode: `push(pop() & pop())`
    CC_IR_OPCODE_AND,
    /// @brief Bitwise or
    /// @details Pseudocode: `push(pop() | pop())`
    CC_IR_OPCODE_OR,
    /// @brief Bitwise xor
    /// @details Pseudocode: `push(pop() ^ pop)`
    CC_IR_OPCODE_XOR,
    /// @brief Bitwise left-shift
    /// @details Pseudocode: `push(pop() << pop())`
    CC_IR_OPCODE_LSH,
    /// @brief Bitwise right-shift
    /// @details Pseudocode: `push(pop() >> pop())`
    CC_IR_OPCODE_RSH,

    // === Casting ===

    /// @brief Zero-extend an int
    /// @details Pseudocode: `push((uintx_t)pop())`
    CC_IR_OPCODE_ZEXT,
    /// @brief Sign-extend an int
    /// @details Pseudocode: `push((intx_t)pop())`
    CC_IR_OPCODE_SEXT,

    // === Control flow ===

    /// @brief Call address
    /// @details: Pseudocode: `call pop()`
    CC_IR_OPCODE_CALL,
    /// @brief Jump to address
    /// @details Pseudocode: `goto pop()`
    CC_IR_OPCODE_JMP,
    /// @brief Jump to block if value is zero
    /// @details Pseudocode: `if (pop() == 0) goto block`
    CC_IR_OPCODE_JZ,
    /// @brief Jump to block if value is not zero
    /// @details Pseudocode: `if (pop() != 0) goto block`
    CC_IR_OPCODE_JNZ,
    /// @brief Return
    /// @details Pseudocode: `return`
    CC_IR_OPCODE_RET,

    // === VM-specific instructions ===

    /// @brief Interrupt the VM with a 32-bit user-defined code
    /// @details Pseudocode: `set_interrupt_exception(vm, ins->operand.u32)`
    CC_IR_OPCODE_INT,
    /// @brief Set the stack frame and allocate constant space
    /// @details Pseudocode: `vm->frame_pointer = stack_alloc(vm, ins->operand.u32)`
    CC_IR_OPCODE_FRAME,

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
    /// @brief Global variable
    CC_IR_OPERAND_SYMBOLID,
    /// @brief Block
    CC_IR_OPERAND_BLOCKID,
    /// @brief Data size of operation (in bytes)
    CC_IR_OPERAND_DATASIZE,
    /// @brief New data size for integer extension (in bytes)
    CC_IR_OPERAND_EXTEND_DATASIZE,
    /// @brief A raw 32-bit constant
    CC_IR_OPERAND_U32,
};

/// @brief Basic types recognized by the IR
enum cc_ir_typeid
{
    CC_IR_TYPEID_FUNC,
    CC_IR_TYPEID_INT,
    CC_IR_TYPEID_FLOAT,
    CC_IR_TYPEID_DATA,
    CC_IR_TYPEID_PTR,
};

enum cc_ir_symbolflag
{
    /// @brief Symbol is a reference outside the object
    CC_IR_SYMBOLFLAG_EXTERNAL = 1 << 0,
    /// @brief Symbol is exposed (if internal) or resolved (if external) at runtime
    /// @details Incompatible with @ref CC_IR_SYMBOLFLAG_EXTERNAL
    CC_IR_SYMBOLFLAG_RUNTIME = 1 << 1,
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
     * @details Pointer belongs to the struct must be freed.
     */
    char* name;
    /** 
     * @brief Size in bytes. Only used for ints, floats, and raw data.
     * 
     * A pointer size is only known while compiling the IR.
     * A function or block's size is only known after partial or full compilation of the IR. 
     */
    cc_ir_datasize data_size;
    /// @brief A value from @ref cc_ir_typeid
    uint8_t typeid;
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
        cc_ir_symbolid symbolid;
        cc_ir_blockid blockid;
        /// @brief Size to extend an int (in bytes)
        cc_ir_datasize extend_data_size;
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
    /// @brief (optional) Name. May be `NULL`.
    /// @details Pointer belongs to the block and must be freed.
    char* name;
    /// @brief A reallocating array of instructions
    cc_ir_ins* ins;
    size_t num_ins;
    /// @brief The next block in the list. May be `NULL`.
    struct cc_ir_block* next_block;
    cc_ir_blockid blockid;
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
    /// @brief This function's corresponding symbol ID in the parent @ref cc_ir_object
    cc_ir_symbolid symbolid;
    cc_ir_localid _next_localid;
    cc_ir_blockid _next_blockid;
} cc_ir_func;

/**
 * @brief A platform-independent offset or size.
 * 
 * The total size for the current platform would be `num_bytes + num_ptrs * sizeof(void*)`
 */
typedef struct cc_ir_offset
{
    /// @brief Number of bytes (not including pointers)
    size_t num_bytes;
    /// @brief Number of pointers
    size_t num_ptrs;
} cc_ir_typesize;

/// @brief A global symbol, which may be external or internal to the current object
typedef struct cc_ir_symbol
{
    /// @brief Name. Optional for internal symbols.
    /// @details Pointer belongs to the struct and must be freed.
    char* name;
    size_t name_len;
    /// @brief A combination of values from @ref cc_ir_symbolflag
    uint8_t symbol_flags;
    /// @brief Pointer to the value of the symbol. Valid when `!(symbol_flags & CC_IR_SYMBOLFLAG_EXTERNAL)`
    union {
        cc_ir_func* func;
    } ptr;
    /// @brief Symbol ID in the parent @ref cc_ir_object
    cc_ir_symbolid symbolid;
} cc_ir_symbol;

/**
 * @brief An IR object. Similar to a compiler object.
 *
 * An object will contain internal symbols and references to external symbols.
 */
typedef struct cc_ir_object
{
    cc_ir_symbol* symbols;
    size_t num_symbols;
    cc_ir_symbolid _next_symbolid;
} cc_ir_object;

/// @brief Array of every IR instruction's format, ordered by opcode
const extern cc_ir_ins_format cc_ir_ins_formats[CC_IR_OPCODE__COUNT];

void cc_ir_object_create(cc_ir_object* obj);
void cc_ir_object_destroy(cc_ir_object* obj);
/// @brief Find a symbol by ID
cc_ir_symbol* cc_ir_object_get_symbolid(const cc_ir_object* obj, cc_ir_symbolid symbolid);
/// @brief Find a symbol by name
/// @param name Name of the symbol
/// @param name_len Name length. Use `(size_t)-1` for strlen.
cc_ir_symbol* cc_ir_object_get_symbolname(const cc_ir_object* obj, const char* name, size_t name_len);
/// @brief Add a symbol
/// @param name Name for the symbol. String is copied.
/// @param name_len Name length. Use `(size_t)-1` for strlen.
cc_ir_symbolid cc_ir_object_add_symbol(cc_ir_object* obj, const char* name, size_t name_len, cc_ir_symbol** out_symbolptr);
/**
 * @brief Import a symbol by name
 * @param is_runtime If true, the symbol is linked at runtime
 * @param name Symbol name
 * @param name_len Name length. Use `(size_t)-1` for strlen.
 */
cc_ir_symbolid cc_ir_object_import(cc_ir_object* obj, bool is_runtime, const char* name, size_t name_len);
/// @brief Add a function to the object
/// @param name Name for the symbol. String is copied.
/// @param name_len Name length. Use `(size_t)-1` for strlen.
cc_ir_func* cc_ir_object_add_func(cc_ir_object* obj, const char* name, size_t name_len);

/// @brief Create a symbol
/// @param name Name for the symbol. String is copied.
/// @param name_len Name length. Use `(size_t)-1` for strlen
void cc_ir_symbol_create(cc_ir_symbol* symbol, cc_ir_symbolid symbolid, const char* name, size_t name_len);
void cc_ir_symbol_destroy(cc_ir_symbol* symbol);

/// @brief Create a new IR function with a new empty entry block
/// @param symbolid A unique symbolid in the parent @ref cc_ir_object
cc_ir_func* cc_ir_func_create(cc_ir_symbolid symbolid);
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
cc_ir_block* cc_ir_func_getblock(const cc_ir_func* func, cc_ir_blockid blockid);
/**
 * @brief Create a new block following a previous one
 * @param prev Previous block
 * @param name (optional) Block name. Will be copied to a new string.
 * @param name_len (optional) Name length. Use `(size_t)-1` for strlen.
 * @return A new block
 */
cc_ir_block* cc_ir_func_insert(cc_ir_func* func, cc_ir_block* prev, const char* name, size_t name_len);
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
 * @brief Create a block
 * @param blockid An ID, unique within its parent function
 * @param name (optional) Name. May be NULL. Data is copied into a new string.
 * @param name_len (optional) Name length. Use `(size_t)-1` for automatic string length.
 */
cc_ir_block* cc_ir_block_create(cc_ir_blockid blockid, const char* name, size_t name_len);
/// @brief Free the block and its data
void cc_ir_block_destroy(cc_ir_block* block);

/**
 * @brief Insert or append a new instruction anywhere in the block
 * @param index An index, where `index <= block->num_ins`
 * @param ins The instruction
 */
void cc_ir_block_insert(cc_ir_block* block, size_t index, const cc_ir_ins* ins);
/// @brief Append an instruction to the block
/// @return The appended instruction's index
static inline size_t cc_ir_block_append(cc_ir_block* block, const cc_ir_ins* ins)
{
    cc_ir_block_insert(block, block->num_ins, ins);
    return block->num_ins - 1;
}

void cc_ir_block_argp(cc_ir_block* block);
void cc_ir_block_addrl(cc_ir_block* block, cc_ir_localid localid);
void cc_ir_block_sizel(cc_ir_block* block, cc_ir_datasize data_size, cc_ir_localid localid);
void cc_ir_block_loadl(cc_ir_block* block, cc_ir_localid localid);
void cc_ir_block_addrg(cc_ir_block* block, cc_ir_symbolid symbolid);
void cc_ir_block_sizep(cc_ir_block* block, cc_ir_datasize data_size);
void cc_ir_block_iconst(cc_ir_block* block, cc_ir_datasize data_size, int32_t value);
void cc_ir_block_uconst(cc_ir_block* block, cc_ir_datasize data_size, uint32_t value);
void cc_ir_block_load(cc_ir_block* block, cc_ir_datasize data_size);
void cc_ir_block_store(cc_ir_block* block, cc_ir_datasize data_size);
void cc_ir_block_dupe(cc_ir_block* block, cc_ir_datasize data_size);
void cc_ir_block_add(cc_ir_block* block, cc_ir_datasize data_size);
void cc_ir_block_sub(cc_ir_block* block, cc_ir_datasize data_size);
void cc_ir_block_mul(cc_ir_block* block, cc_ir_datasize data_size);
void cc_ir_block_umul(cc_ir_block* block, cc_ir_datasize data_size);
void cc_ir_block_div(cc_ir_block* block, cc_ir_datasize data_size);
void cc_ir_block_udiv(cc_ir_block* block, cc_ir_datasize data_size);
void cc_ir_block_mod(cc_ir_block* block, cc_ir_datasize data_size);
void cc_ir_block_umod(cc_ir_block* block, cc_ir_datasize data_size);
void cc_ir_block_neg(cc_ir_block* block, cc_ir_datasize data_size);
void cc_ir_block_not(cc_ir_block* block, cc_ir_datasize data_size);
void cc_ir_block_and(cc_ir_block* block, cc_ir_datasize data_size);
void cc_ir_block_or(cc_ir_block* block, cc_ir_datasize data_size);
void cc_ir_block_xor(cc_ir_block* block, cc_ir_datasize data_size);
void cc_ir_block_lsh(cc_ir_block* block, cc_ir_datasize data_size);
void cc_ir_block_rsh(cc_ir_block* block, cc_ir_datasize data_size);
void cc_ir_block_zext(cc_ir_block* block, cc_ir_datasize data_size, cc_ir_datasize extend_data_size);
void cc_ir_block_sext(cc_ir_block* block, cc_ir_datasize data_size, cc_ir_datasize extend_data_size);
void cc_ir_block_call(cc_ir_block* block);
void cc_ir_block_jmp(cc_ir_block* block);
void cc_ir_block_jz(cc_ir_block* block, cc_ir_datasize data_size, const cc_ir_block* dst);
void cc_ir_block_jnz(cc_ir_block* block, cc_ir_datasize data_size, const cc_ir_block* dst);
void cc_ir_block_ret(cc_ir_block* block);
void cc_ir_block_int(cc_ir_block* block, uint32_t interrupt_code);
