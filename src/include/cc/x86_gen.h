#pragma once
#include "lib.h"
#include "x86_asm.h"

/**
 * @file
 * @brief Generate x86 code from IR.
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

/// @brief The mapping of a local value to an x86 register
typedef struct x86local
{
    /// @brief A value from @ref x86_reg_enum
    uint8_t reg;
    /// @brief Offsets the register value. If non-zero, the register is de-referenced.
    int32_t offset;
} x86local;

typedef struct x86gen
{
    /// @brief A value from @ref x86_mode
    uint8_t mode;
    /// @brief Calling convention followed in the prolog and epilog
    const x86conv* conv;
    const struct cc_ir_func* irfunc;
} x86gen;

/// @brief 64-bit __fastcall, the default Windows calling convention
extern const x86conv X86_CONV_WIN64_FASTCALL;
/// @brief 64-bit cdecl, the default SYSV calling convention
extern const x86conv X86_CONV_SYSV64_CDECL;

/**
 * @brief Create an x86-to-IR generator
 * @param conv Calling convention. This defines 
 */ 
void x86gen_create(x86gen* gen, const x86conv* conv, const struct cc_ir_func* irfunc);
void x86gen_destroy(x86gen* gen);
