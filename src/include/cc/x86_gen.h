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
 * @param input The input function
 * @param output An uninitialized struct to store the result
 */
void x86gen_simplify(const x86gen* gen, const cc_ir_func* input, cc_ir_func* output);
/**
 * @brief Dump the IR straight to x86 code.
 * @param func An uninitialized struct to store the result
 */
void x86gen_dump(x86gen* gen, x86func* func);