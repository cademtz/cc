#pragma once
#include "lib.h"

/**
 * @file
 * @brief An x86/AMD64 assembler.
 * 
 * The API is similar to AsmJit. It uses function calls instead of assembling text.
 * 
 * Basics
 * -----------------
 * An assembler is created with an execution mode ( @ref x86_mode ).
 * This sample will assemble `add rax, r15`:
 * ```
 *      x86func func;
 *      x86func_create(&func, X86_MODE_LONG);
 *      x86func_add(&func, X86_OPSIZE_QWORD, x86_reg(X86_REG_A), x86_reg(X86_REG_R15));
 * ```
 * The value @ref X86_OPSIZE_QWORD is an explicit override to add two QWORDS.
 * Similarly, using @ref X86_OPSIZE_BYTE would add two bytes.
 * This sample will assemble `add al, r15b`:
 * ```
 *      x86func_add(&func, X86_OPSIZE_BYTE, x86_reg(X86_REG_A), x86_reg(X86_REG_R15));
 * ```
 * You may use the less explicit size @ref X86_OPSIZE_DEFAULT (0).
 * 
 * Labels and jumps
 * -----------------
 * Labels are required for conditional jumps. They are also more convenient.
 * 
 * You may create a label at any time:
 * ```
 *      x86label exit = x86func_newlabel(&func);
 * ```
 * You may jump to a label at any time:
 * ```
 *      x86func_cmp(&func, 0, x86_reg(X86_REG_A), x86_const(0));
 *      x86func_jne(&func, exit); // if (eax != 0) goto exit
 * ```
 * You must eventually place the label and add code:
 * ```
 *      x86func_label(&func, exit); // This label will lead to the code below
 *      x86func_ret(&func);
 * ```
 * It is undefined behavior to place a label multiple times or to jump a label that is never placed.
 */

/// @brief x86 execution mode
enum x86_mode
{
    /// @brief 16-bit code
    X86_MODE_REAL,
    /// @brief 32-bit code
    X86_MODE_PROTECTED,
    /// @brief 64-bit code
    X86_MODE_LONG,
};

/**
 * @brief x86 register indexes.
 * 
 * Registers are in groups of 8, in the same order as required in the ModRM byte.
 * Mask the index with 3 bits (or modulo 8) to get the ModRM index for that register.
 */
enum x86_reg_enum
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

    /// @brief The number of (supported) x86 registers
    X86_NUM_REGISTERS
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

enum x86_opsize
{
    /**
     * @brief The default size, according to the current execution mode.
     * 
     * - `X86_MODE_REAL`:      WORD
     * - `X86_MODE_PROTECTED`: DWORD
     * - `X86_MODE_LONG`:      DWORD
     */
    X86_OPSIZE_DEFAULT,
    X86_OPSIZE_BYTE,
    X86_OPSIZE_WORD,
    /// @brief Only effective in long mode
    X86_OPSIZE_DWORD,
    /// @brief Only effective in long mode
    X86_OPSIZE_QWORD,
};

enum x86_operand_type
{
    /// @brief A register
    X86_OPERAND_REG,
    /// @brief A de-referenced register, with optional scale, index, and offset
    X86_OPERAND_MEM,
    /// @brief A de-referenced memory offset
    X86_OPERAND_OFFSET,
    /// @brief An constant value
    X86_OPERAND_CONST,
};

/**
 * @brief A high-level operand representation.
 * 
 * Different types of operands can be constructed with the following helpers:
 * - @ref x86_reg    : Register
 * - @ref x86_deref  : De-referenced register
 * - @ref x86_index  : De-referenced register with index and offset (such as `[eax+ecx*8+72]`)
 * - @ref x86_offset : De-referenced memory offset (Such as `[ds:0x1000]` or `[rip+0x1000]`)
 * - @ref x86_const  : A constant integer value
 */
typedef struct x86operand
{
    /// @brief A value from @ref x86_reg_enum.
    /// This may be the base of a memory operand.
    uint8_t reg;
    /// @brief A value from @ref x86_operand_type
    uint8_t type;
    /// @brief A value from @ref x86_sib_scale
    uint8_t scale;
    /// @brief A value from @ref x86_reg_enum.
    /// @ref X86_REG_SP means no index will be used.
    uint8_t index;
    /**
     * @brief A memory offset or const value, depending on @ref type.
     * 
     * - type = @ref X86_OPERAND_CONST : This field is a const value
     * - type = @ref X86_OPERAND_OFFSET : This field is a memory offset
     */
    int32_t offset;
} x86operand;

/// @brief The location of an immediate value in function code
typedef struct x86imm
{
    /// @brief Offset from the start of the function
    uint32_t offset;
    /// @brief Size, in bytes
    uint8_t size;
} x86imm;

/// @brief The ID of a code label
typedef uint16_t x86label;

/// @brief An offset to the code referencing a label
typedef struct x86labelref
{
    /// @brief The immediate to be modified
    x86imm imm;
    /// @brief Code offset of the next IP
    uint32_t next_ip;
    /// @brief The label being referenced
    x86label label;
} x86labelref;

typedef struct x86func
{
    /// @brief x86 code
    uint8_t* code;
    /// @brief Number of bytes in @ref code
    size_t size_code;
    /// @brief Location to append or overwrite code
    size_t writepos;

    /// @brief Locations of all labels in code
    uint32_t* labels;
    /// @brief Number of items in @ref labels
    x86label num_labels;

    /// @brief Array of references to labels
    x86labelref* labelrefs;
    /// @brief Number of items in @ref labelrefs
    size_t num_labelrefs;

    /// @brief Info on the last instruction's left-operand immediate (if any)
    x86imm lhs_imm;
    /// @brief Info on the last instruction's right-operand immediate (if any)
    x86imm rhs_imm;
    /// @brief A value from @ref x86_mode
    uint8_t mode;
} x86func;

/// @brief Compare two operands
/// @return Zero if `a == b`. Negative when `a > b`. Positive when `a > b`
int x86operand_cmp(x86operand a, x86operand b);

/// @brief Create a new function with one block
/// @param mode A value from @ref x86_mode
void x86func_create(x86func* func, uint8_t mode);
void x86func_destroy(x86func* func);
/// @brief Create a new code label
x86label x86func_newlabel(x86func* func);
/**
 * @brief Place `label` at the current location in code.
 * 
 * It is undefined to call this at different locations.
 */
void x86func_label(x86func* func, x86label label);

/// @brief Emit an 8-bit immediate value
void x86func_imm8(x86func* func, uint8_t byte);
/// @brief Emit a 16-bit immediate value
void x86func_imm16(x86func* func, uint16_t imm);
/// @brief Emit a 32-bit immediate value
void x86func_imm32(x86func* func, uint32_t imm);
/// @brief Emit a 64-bit immediate value
void x86func_imm64(x86func* func, uint64_t imm);

/// @brief Emit: `push src`
/// @param opsize Must be @ref X86_OPSIZE_DEFAULT or @ref X86_OPSIZE_WORD
void x86func_push(x86func* func, uint8_t opsize, x86operand src);
/// @brief Emit: `pop dst`
/// @param opsize Must be @ref X86_OPSIZE_DEFAULT or @ref X86_OPSIZE_WORD
/// @param dst Register or memory
void x86func_pop(x86func* func, uint8_t opsize, x86operand dst);
/// @brief Emit: `add dst, src`
void x86func_add(x86func* func, uint8_t opsize, x86operand dst, x86operand src);
/// @brief Emit: `sub dst, src`
void x86func_sub(x86func* func, uint8_t opsize, x86operand dst, x86operand src);
/// @brief Emit: `mul src`
/// @param src Register or memory
void x86func_mul(x86func* func, uint8_t opsize, x86operand src);
/// @brief Emit: `imul src`
/// @param src The multiplicand, a register or memory
void x86func_imul(x86func* func, uint8_t opsize, x86operand src);
/// @brief Emit: `imul dst, src` (2 operands)
/// @param opsize All values except @ref X86_OPSIZE_BYTE are supported
/// @param dst The multiplicand, a value from @ref x86_reg_enum
/// @param src The multiplier, any operand
void x86func_imul2(x86func* func, uint8_t opsize, uint8_t dst, x86operand src);
/// @brief Emit: `imul dst, lhs, rhs` (3 operands)
/// @param dst A value from @ref x86_reg_enum
/// @param lhs The multiplicand, a register or memory
/// @param rhs The multiplier
/// @param opsize All values except @ref X86_OPSIZE_BYTE are supported
void x86func_imul3(x86func* func, uint8_t opsize, uint8_t dst, x86operand lhs, int32_t rhs);
/// @brief Emit: `idiv src`
/// @param src Register or memory
void x86func_idiv(x86func* func, uint8_t opsize, x86operand src);
/// @brief Emit: `div src`
/// @param src Register or memory
void x86func_div(x86func* func, uint8_t opsize, x86operand src);
/// @brief Emit: `mov dst, src`
/// @param opsize A value from @ref x86_opsize
void x86func_mov(x86func* func, uint8_t opsize, x86operand dst, x86operand src);
/// @brief Emit: `cmp lhs, rhs`, where `lhs` is always a non-const operand
void x86func_cmp(x86func* func, uint8_t opsize, x86operand lhs, x86operand rhs);
void x86func_jmp(x86func* func, x86label label);
/// @brief Emit: `jo label`
void x86func_jo(x86func* func, x86label label);
/// @brief Emit: `jno label`
void x86func_jno(x86func* func, x86label label);
/// @brief Emit: `jc label`
void x86func_jc(x86func* func, x86label label);
/// @brief Alias for @ref x86func_jc
static inline void x86func_jb(x86func* func, x86label label) { x86func_jc(func, label); }
/// @brief Alias for @ref x86func_jc
static inline void x86func_jnae(x86func* func, x86label label) { x86func_jc(func, label); }
/// @brief Emit: `jnc label`
void x86func_jnc(x86func* func, x86label label);
/// @brief Alias for @ref x86func_jnc
static inline void x86func_jnb(x86func* func, x86label label) { x86func_jnc(func, label); }
/// @brief Alias for @ref x86func_jnc
static inline void x86func_jae(x86func* func, x86label label) { x86func_jnc(func, label); }
/// @brief Emit: `jz label`
void x86func_jz(x86func* func, x86label label);
/// @brief Alias for @ref x86func_jz
static inline void x86func_je(x86func* func, x86label label) { x86func_jz(func, label); }
/// @brief Emit: `jnz label`
void x86func_jnz(x86func* func, x86label label);
/// @brief Alias for @ref x86func_jnz
static inline void x86func_jne(x86func* func, x86label label) { x86func_jnz(func, label); }
/// @brief Emit: `jbe label`
void x86func_jbe(x86func* func, x86label label);
/// @brief Alias for @ref x86func_jbe
static inline void x86func_jna(x86func* func, x86label label) { x86func_jbe(func, label); }
/// @brief Emit: `jnbe label`
void x86func_jnbe(x86func* func, x86label label);
/// @brief Alias for @ref x86func_jnbe
static inline void x86func_ja(x86func* func, x86label label) { x86func_jnbe(func, label); }
/// @brief Emit: `js label`
void x86func_js(x86func* func, x86label label);
/// @brief Emit: `jns label`
void x86func_jns(x86func* func, x86label label);
/// @brief Emit: `jp label`
void x86func_jp(x86func* func, x86label label);
/// @brief Alias for @ref x86func_jp
static inline void x86func_jpe(x86func* func, x86label label) { x86func_jp(func, label); }
/// @brief Emit: `jnp label`
void x86func_jnp(x86func* func, x86label label);
/// @brief Alias for @ref x86func_jnp
static inline void x86func_jpo(x86func* func, x86label label) { x86func_jnp(func, label); }
/// @brief Emit: `jl label`
void x86func_jl(x86func* func, x86label label);
/// @brief Alias for @ref x86func_jl
static inline void x86func_jnge(x86func* func, x86label label) { x86func_jl(func, label); }
/// @brief Emit: `jnl label`
void x86func_jnl(x86func* func, x86label label);
/// @brief Alias for @ref x86func_jnl
static inline void x86func_jge(x86func* func, x86label label) { x86func_jnl(func, label); }
/// @brief Emit: `jle label`
void x86func_jle(x86func* func, x86label label);
/// @brief Alias for @ref x86func_jle
static inline void x86func_jng(x86func* func, x86label label) { x86func_jle(func, label); }
/// @brief Emit: `jnle label`
void x86func_jnle(x86func* func, x86label label);
/// @brief Alias for @ref x86func_jnle
static inline void x86func_jg(x86func* func, x86label label) { x86func_jnle(func, label); }
/// @brief Emit: `ret`
void x86func_ret(x86func* func);

/**
 * @brief Encode a ModRM byte
 * @param mod A value from @ref x86_mod
 * @param reg A value from @ref x86_reg_enum, which will be wrapped from 0-7.
 * This may also be the `/digit` number in the AMD64 manual.
 * @param rm A value from @ref x86_reg_enum, which will be wrapped from 0-7
 */
static inline uint8_t x86_modrm(uint8_t mod, uint8_t reg, uint8_t rm)
{
    reg &= 7;
    rm &= 7;
    return rm | (reg << 3) | (mod << 6);
}
/**
 * @brief Encode an SIB byte
 * @param scale A value from @ref x86_sib_scale
 * @param index A value from @ref x86_reg_enum, which will be wrapped from 0-7.
 * A value of 4 ( @ref X86_REG_SP ) indicates no index.
 * @param base A value from @ref x86_reg_enum, which will be wrapped from 0-7
 */
static inline uint8_t x86_sib(uint8_t scale, uint8_t index, uint8_t base)
{
    index &= 7;
    base &= 7;
    return base | (index << 3) | (scale << 6);
}

/// @brief Make a plain register operand
static inline x86operand x86_reg(uint8_t reg)
{
    x86operand rm;
    memset(&rm, 0, sizeof(rm));
    rm.type = X86_OPERAND_REG;
    rm.reg = reg;
    return rm;
}
/// @brief Make a de-referenced register operand
static inline x86operand x86_deref(uint8_t reg)
{
    x86operand rm;
    memset(&rm, 0, sizeof(rm));
    rm.type = X86_OPERAND_MEM;
    rm.reg = reg;
    rm.scale = X86_SIB_SCALE_1;
    rm.index = X86_REG_SP; // SP means no index will be used
    return rm;
}
/**
 * @brief Make an indexed register operand: `[reg + index_reg*scale + offset]`
 * @param reg The base register
 * @param index_reg An index. If the value is @ref X86_REG_SP, then no index is used.
 * @param scale A value from @ref x86_sib_scale
 * @param offset A constant offset
 */
static inline x86operand x86_index(uint8_t reg, uint8_t index_reg, uint8_t scale, int32_t offset)
{
    x86operand rm;
    memset(&rm, 0, sizeof(rm));
    rm.type = X86_OPERAND_MEM;
    rm.reg = reg;
    rm.index = index_reg;
    rm.scale = scale;
    rm.offset = offset;
    return rm;
}
/**
 * @brief Make a de-referenced address operand.
 * 
 * In long mode, this represents a sign-extended offset: `[RIP+offset]`.
 * Otherwise, this represents `ds:offset`.
 */
static inline x86operand x86_offset(int32_t offset)
{
    x86operand rm;
    memset(&rm, 0, sizeof(rm));
    rm.type = X86_OPERAND_OFFSET;
    rm.offset = offset;
    return rm;
}
/// @brief Make a sign-extended integer operand
static inline x86operand x86_const(int32_t value)
{
    x86operand rm;
    memset(&rm, 0, sizeof(rm));
    rm.type = X86_OPERAND_CONST;
    rm.offset = value;
    return rm;
}
/// @brief Get the size of a pointer for a given execution mode
/// @param mode A value from @ref x86_mode
static inline uint8_t x86_ptrsize(uint8_t mode)
{
    uint8_t sizes[] = { 2, 4, 8 };
    return sizes[mode];
}
