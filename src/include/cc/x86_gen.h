#pragma once
#include "lib.h"

/// @brief x86 execution mode
enum x86_mode
{
    /// @brief 16-bit
    X86_MODE_REAL,
    /// @brief 32-bit
    X86_MODE_PROTECTED,
    /// @brief 64-bit
    X86_MODE_LONG,
};

/**
 * @brief x86 register indexes.
 * 
 * Registers are in groups of 8, in the same order as required in the ModRM byte.
 * Mask the index with 3 bits (or modulo 8) to get the ModRM index for that register.
 */
enum x86_reg
{
    X86_REG_A,
    X86_REG_C,
    X86_REG_D,
    X86_REG_B,
    X86_REG_SP,
    X86_REG_BP,
    X86_REG_SI,
    X86_REG_DI,

    // Extended registers (Long mode only, requires the REX prefix)
    X86_REG_R8, X86_REG_R9, X86_REG_R10, X86_REG_R11,
    X86_REG_R12, X86_REG_R13, X86_REG_R14, X86_REG_R15,

    // 128-bit registers
    X86_REG_XMM0, X86_REG_XMM1, X86_REG_XMM2, X86_REG_XMM3,
    X86_REG_XMM4, X86_REG_XMM5, X86_REG_XMM6, X86_REG_XMM7,

    // Extended 128-bit registers (Long mode only, requires a prefix)
    X86_REG_XMM8, X86_REG_XMM9, X86_REG_XMM10, X86_REG_XMM11,
    X86_REG_XMM12, X86_REG_XMM13, X86_REG_XMM14, X86_REG_XMM15,
};

/// @brief x86 operand mode (the 'Mod' in 'ModRM')
enum x86_mod
{
    X86_MOD_DISP0,
    X86_MOD_DISP8,
    X86_MOD_DISP32,
    X86_MOD_DIRECT,
};

/// @brief x86 operand scale/index
enum x86_sib_scale
{
    X86_SIB_SCALE_1,
    X86_SIB_SCALE_2,
    X86_SIB_SCALE_4,
    X86_SIB_SCALE_8,

    //X86_SIB_INDEX_NONE = X86_REG_SP,
    //X86_SIB_BASE_NONE = X86_REG_SP,
};

/// @brief x86-64 REX prefix
enum x86_rex
{
    /// @brief Extend ModRM.rm, SIB.base, or opcode.reg (`+r` in the AMD64 manual)
    X86_REX_B = 1 << 0,
    /// @brief Extend SIB.index
    X86_REX_X = 1 << 1,
    /// @brief Extend ModRM.reg
    X86_REX_R = 1 << 2,
    /// @brief 64-bit operand size
    X86_REX_W = 1 << 3,
    /// @brief The high nibble of every REX prefix
    X86_REX__REX = 0x40,
};

/// @brief A high-level register/memory operand representation
typedef struct x86regmem
{
    /// @brief A value from @ref x86_reg.
    /// This may be the base of a memory operand.
    uint8_t reg;
    /// @brief If true, then the scale, index, and offset are used to dereference `reg`
    uint8_t is_memory;
    /// @brief A value from @ref x86_sib_scale
    uint8_t scale;
    /// @brief A value from @ref x86_reg.
    /// @ref X86_REG_SP means no index will be used.
    uint8_t index;
    int32_t offset;
} x86_binop;

/// @brief The properties of an x86 register
typedef struct x86reg
{
    const char* name;
    /// @brief Register size, in bytes
    uint8_t size;
    /// @brief `true` if a callee may change this register
    bool is_volatile;
} x86reg;

/// @brief A configurable x86 generator
typedef struct x86gen
{
    /// @brief A value from @ref x86_mode
    uint8_t mode;
    
} x86gen;

typedef struct x86link
{
    uint32_t code_offset;
} x86link;

typedef struct x86func
{
    /// @brief x86 code
    uint8_t* code;
    /// @brief Each block's offset from the entrypoint, in order
    size_t* blocks;
    size_t size_code;
    size_t num_blocks;
} x86func;

/// @brief Create a new function with one block
void x86func_create(x86func* func);
void x86func_destroy(x86func* func);

/// @brief Begin a new block in the function. If there is no code, this will have no effect.
void x86func_block(x86func* func);
/// @brief Emit one byte
void x86func_byte(x86func* func, uint8_t byte);
/// @brief Emit a 16-bit immediate value
void x86func_imm16(x86func* func, uint16_t imm);
/// @brief Emit a 32-bit immediate value
void x86func_imm32(x86func* func, uint32_t imm);
/// @brief Emit a 64-bit immediate value
void x86func_imm64(x86func* func, uint64_t imm);

void x86func_add(x86func* func, x86regmem dst, x86regmem src);
void x86func_ret(x86func* func);

/**
 * @brief Encode a ModRM byte
 * @param mod A value from @ref x86_mod
 * @param reg A value from @ref x86_reg, which will be wrapped from 0-7.
 * This may also be the `/digit` number in the AMD64 manual.
 * @param rm A value from @ref x86_reg, which will be wrapped from 0-7
 */
static uint8_t x86_modrm(uint8_t mod, uint8_t reg, uint8_t rm)
{
    reg &= 7;
    rm &= 7;
    return rm | (reg << 3) | (mod << 6);
}
/**
 * @brief Encode an SIB byte
 * @param scale A value from @ref x86_sib_scale
 * @param index A value from @ref x86_reg, which will be wrapped from 0-7.
 * A value of 4 ( @ref X86_REG_SP ) indicates no index.
 * @param base A value from @ref x86_reg, which will be wrapped from 0-7
 */
static uint8_t x86_sib(uint8_t scale, uint8_t index, uint8_t base)
{
    index &= 7;
    base &= 7;
    return base | (index << 3) | (scale << 6);
}
