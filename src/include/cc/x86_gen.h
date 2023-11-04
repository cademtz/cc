#pragma once
#include "lib.h"
#include "x86_asm.h"
#include "ir.h"

/**
 * @file
 * @brief Generate x86 code from IR.
 * 
 * Default calling conventions are provided:
 * - @ref X86_CONV_WIN64_FASTCALL
 * - @ref X86_CONV_SYSV64_CDECL
 */

/// @brief A calling convention. This also defines the ABI.
typedef struct x86conv
{
    /// @brief Array indicating which registers are volatile. Each index is a value from @ref x86_reg_enum.
    bool volatiles[X86_NUM_REGISTERS];
    /**
     * @brief Amount of stack reserved before stack-arguments are pushed.
     * 
     * This is the 32-byte "shadow space" in the Windows x64 ABI.
     */
    uint32_t stack_preargs;
    /// @brief Amount of stack reserved after stack-arguments are pushed
    uint32_t stack_postargs;
    /// @brief If true, the function will not return
    bool noreturn;
} x86conv;

enum x86_varmap_usage
{
    /// @brief The location is unused
    X86_VARMAP_UNUSED,
    /// @brief The location is in temporary use
    X86_VARMAP_TEMP,
    /// @brief The location is in permanent use
    X86_VARMAP_PERM,
};

/**
 * @brief A map of the permanent and temporary location of every local.
 * 
 * First, locals are given permanent storage.
 * This stores a local at `[sp-4]`:
 * ```c
 *     x86varmap_setperm(map, localid, x86_index(X86_REG_SP, X86_REG_SP, 0, -4));
 * ```
 * Next, a temporary location may be given.
 * This indicates `localid` has a copy in `rax`:
 * ```c
 *     x86varmap_settemp(map, localid, x86_reg(X86_REG_A));
 * ```
 * Registers are preferred as temp locations.
 */
typedef struct x86varmap
{
    /// @brief Map localid -> `vec_perm` index
    cc_hmap32 map_perm;
    /// @brief Map localid -> `vec_temp` index
    cc_hmap32 map_temp;
    /// @brief Vector of @ref x86operand, where a local is permanently stored.
    cc_vec vec_perm;
    /// @brief Vector of @ref x86operand, where a local is temporarily stored.
    cc_vec vec_temp;
    /// @brief Required amount of stack
    uint32_t stack_size;
} x86varmap;

void x86varmap_create(x86varmap* map);
void x86varmap_destroy(x86varmap* map);
/// @brief Reset the map without shrinking the capacity
void x86varmap_clear(x86varmap* map);
/// @brief Find whether a location is in use
/// @return A value from @ref x86_varmap_usage
uint8_t x86varmap_usage(const x86varmap* map, x86operand location);
/// @brief Set the permanent location of a local. Multiple locals may have the same location.
void x86varmap_setperm(x86varmap* map, cc_ir_localid localid, x86operand location);
/// @brief Set the temp location of a local. Multiple locals may have the same location.
void x86varmap_settemp(x86varmap* map, cc_ir_localid localid, x86operand location);
/// @brief Invalidate all temp copies
void x86varmap_cleartemp(x86varmap* map);
/// @brief Get the permanent location of a local
x86operand x86varmap_getperm(const x86varmap* map, cc_ir_localid localid);
/// @brief Get the temporary location of a local
x86operand x86varmap_gettemp(const x86varmap* map, cc_ir_localid localid);
/// @brief Invalidate all temp copies of a local, implying the permanent location was written to
void x86varmap_update(x86varmap* map, cc_ir_localid localid);
/// @brief Mark `location` as overwritten, no longer containing any local
void x86varmap_overwrite(x86varmap* map, x86operand location);

typedef struct x86gen
{
    /// @brief A value from @ref x86_mode
    uint8_t mode;
    /// @brief The conventions to uphold in our function. Called functions can differ.
    const x86conv* conv;
    /**
     * @brief The IR function to compile.
     * 
     * This function has been transformed to meet special requirements for x86.
     * @see x86gen_simplify
     */
    cc_ir_func irfunc;
    /// @brief The current output for x86 code
    x86func* func;
    x86varmap varmap;
    /// @brief Map (a block's) localid -> x86label
    cc_hmap32 map_blocks;
    /// @brief Offset from initial SP value
    int32_t stack_offset;
} x86gen;

/// @brief 64-bit __fastcall, the default Windows calling convention
extern const x86conv X86_CONV_WIN64_FASTCALL;
/// @brief 64-bit cdecl, the default SYSV calling convention
extern const x86conv X86_CONV_SYSV64_CDECL;

/**
 * @brief Create an x86-to-IR generator
 * @param conv Calling convention. This includes which registers to preserve.
 * @param irfunc The IR function to compile
 */ 
void x86gen_create(x86gen* gen, const x86conv* conv, const cc_ir_func* irfunc);
void x86gen_destroy(x86gen* gen);
/**
 * @brief Transform the `input` function's code to be more similar to x86 logic.
 * 
 * - Binary operations will always read and write `dst` (such as `add dst, dst, rhs`).
 * - Constants are loaded at the start, and never written elsewhere.
 * @param input The input function
 * @param output An uninitialized struct to store the result
 */
void x86gen_simplify(const x86gen* gen, const cc_ir_func* input, cc_ir_func* output);
/**
 * @brief Dump the IR straight to x86 code.
 * 
 * This will make heavy assumptions about `gen->irfunc`:
 * - Binary operations are always performed on the destination value (`op dst, dst, rhs`)
 * - Two constants are never used together (`add i,i,i` will fail if preceeded by `ldc i, 9`)
 * @param func An uninitialized struct to store the result
 */
void x86gen_dump(x86gen* gen, x86func* func);