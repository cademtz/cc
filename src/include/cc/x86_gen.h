#pragma once
#include "lib.h"
#include "x86_asm.h"

/**
 * @file
 * @brief Generate x86 code from IR
 */

/// @brief A calling convention
typedef struct x86conv
{
    /// @brief Array indicating which registers are volatile. Each index is a value from @ref x86_reg_enum.
    bool volatiles[X86_NUM_REGISTERS];
    /**
     * @brief Amount of stack reserved before stack-arguments are pushed
     * 
     * The "shadow space" in Windows x64 ABI
     */
    uint32_t stack_preargs;
    /// @brief Amount of stack reserved after stack-arguments are pushed
    uint32_t stack_postargs;
    /// @brief If true, the function does not return
    bool noreturn;
};

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
    /// @brief 
    const x86conv* conv;
    const struct cc_ir_func* irfunc;
} x86gen;


