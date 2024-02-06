#pragma once
#include <stddef.h>
#include <stdint.h>

/**
 * @file
 * @brief Large integer operations.
 * 
 * - `dst` is the destination and left-hand operand. `src` is the right-hand operand.
 * - All sizes are measured in bytes.
 */

enum cc_bigint_endian
{
    CC_BIGINT_ENDIAN_LITTLE,
    CC_BIGINT_ENDIAN_BIG,
};

enum cc_bigint_endian cc_bigint_endianness();

void cc_bigint_i32(size_t size, void* dst, int32_t src);
void cc_bigint_u32(size_t size, void* dst, uint32_t src);
/// @brief ASCII to integer
/// @param str_len The string length, or `(size_t)-1` for null-terminated strings.
size_t cc_bigint_atoi(size_t size, void* dst, int radix, const char* str, size_t str_len);

/// @brief Read a byte
/// @param byte_index An index starting from the least-significant byte
uint8_t cc_bigint_byte(size_t size, const void* src, size_t byte_index);
const uint8_t* cc_bigint_byteptr_const(size_t size, const void* src, size_t byte_index);
uint8_t* cc_bigint_byteptr(size_t size, void* src, size_t byte_index);

void cc_bigint_add(size_t size, void* dst, const void* src);
void cc__bigint_add_32(size_t size, void* dst, uint32_t src, int sign_bit);
static void cc_bigint_add_i32(size_t size, void* dst, int32_t src) { cc__bigint_add_32(size, dst, (uint32_t)src, src >> 31); }
static void cc_bigint_add_u32(size_t size, void* dst, uint32_t src) { cc__bigint_add_32(size, dst, src, 0); }
/// @brief Unsigned multiplication
void cc_bigint_umul(size_t size, void* dst, const void* src);
/// @brief Unsigned multiplication
void cc_bigint_umul_u32(size_t size, void* dst, uint32_t src);
void cc_bigint_sub(size_t size, void* dst, const void* src);
/// @brief Negative value: `dst = -dst`
void cc_bigint_neg(size_t size, void* dst);

/// @brief Bitwise not
void cc_bigint_not(size_t size, void* dst);

/// @brief Get the sign bit
int cc_bigint_sign(size_t size, const void* lhs);
/// @brief Signed comparison
/// @return 1 if `lhs > rhs`, 0 if `lhs == rhs`, -1 if `lhs < rhs`
int cc_bigint_cmp(size_t size, const void* lhs, const void* rhs);
/// @brief Unsigned comparison
/// @return 1 if `lhs > rhs`, 0 if `lhs == rhs`, -1 if `lhs < rhs`
int cc_bigint_ucmp(size_t size, const void* lhs, const void* rhs);