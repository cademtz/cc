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

// === Constructors ===

void cc_bigint_i32(size_t size, void* dst, int32_t src);
void cc_bigint_u32(size_t size, void* dst, uint32_t src);
/// @brief ASCII to integer
/// @param str_len The string length, or `(size_t)-1` for null-terminated strings.
size_t cc_bigint_atoi(size_t size, void* dst, int radix, const char* str, size_t str_len);

// === Read-only operations ===

/// @brief Get the sign bit
int cc_bigint_sign(size_t size, const void* lhs);
/// @brief Signed comparison
/// @return 1 if `lhs > rhs`, 0 if `lhs == rhs`, -1 if `lhs < rhs`
int cc_bigint_cmp(size_t size, const void* lhs, const void* rhs);
/// @brief Unsigned comparison
/// @return 1 if `lhs > rhs`, 0 if `lhs == rhs`, -1 if `lhs < rhs`
int cc_bigint_ucmp(size_t size, const void* lhs, const void* rhs);
/// @brief Read a bit
uint8_t cc_bigint_bit(size_t size, const void* src, size_t bit_index);
/// @brief Read a byte
/// @param byte_index An index starting from the least-significant byte
uint8_t cc_bigint_byte(size_t size, const void* src, size_t byte_index);
const uint8_t* cc_bigint_byteptr_const(size_t size, const void* src, size_t byte_index);
uint8_t* cc_bigint_byteptr(size_t size, void* src, size_t byte_index);
/// @brief Get the pointer to a span of bytes
/// @param index An index starting from the least-significant byte
/// @param len Number of bytes included in the span
const uint8_t* cc_bigint_spanptr_const(size_t size, const void* src, size_t index, size_t len);
uint8_t* cc_bigint_spanptr(size_t size, void* src, size_t index, size_t len);

// === Arithmetic ===

void cc_bigint_add(size_t size, void* dst, const void* src);
void cc__bigint_add_32(size_t size, void* dst, uint32_t src, int sign_bit);
static void cc_bigint_add_i32(size_t size, void* dst, int32_t src) { cc__bigint_add_32(size, dst, (uint32_t)src, src >> 31); }
static void cc_bigint_add_u32(size_t size, void* dst, uint32_t src) { cc__bigint_add_32(size, dst, src, 0); }
void cc_bigint_sub(size_t size, void* dst, const void* src);
void cc__bigint_sub_32(size_t size, void* dst, uint32_t src, int sign_bit);
static void cc_bigint_sub_i32(size_t size, void* dst, int32_t src) { cc__bigint_sub_32(size, dst, (uint32_t)src, src >> 31); }
static void cc_bigint_sub_u32(size_t size, void* dst, uint32_t src) { cc__bigint_sub_32(size, dst, src, 0); }

/// @brief Signed multiplication
/// @param src Will become a positive number
void cc_bigint_mul(size_t size, void* dst, void* src);
void cc_bigint_mul_i32(size_t size, void* dst, int32_t src);
/// @brief Unsigned multiplication
void cc_bigint_umul(size_t size, void* dst, const void* src);
/// @brief Unsigned multiplication
void cc_bigint_umul_u32(size_t size, void* dst, uint32_t src);
/// @brief Signed division, the C way
/// @param num The numerator. Will become a positive number.
/// @param denom The denominator. Will become a positive number.
/// @param quotient Destination for the quotient
/// @param remainder Destination for the remainder
void cc_bigint_div(size_t size, void* num, void* denom, void* quotient, void* remainder);
/// @brief Unsigned division
/// @param num The numerator
/// @param denom The denominator
/// @param quotient Destination for the quotient
/// @param remainder Destination for the remainder
void cc_bigint_udiv(size_t size, const void* num, const void* denom, void* quotient, void* remainder);
void cc_bigint_sub(size_t size, void* dst, const void* src);
/// @brief Negative value: `dst = -dst`
void cc_bigint_neg(size_t size, void* dst);

// === Bitwise operations ===

/// @brief Bitwise not
void cc_bigint_not(size_t size, void* dst);
/// @brief Bitwise and
void cc_bigint_and(size_t size, void* dst, const void* src);
/// @brief Bitwise or
void cc_bigint_or(size_t size, void* dst, const void* src);
/// @brief Bitwise exclusive-or
void cc_bigint_xor(size_t size, void* dst, const void* src);
/// @brief Bitwise left-shift. Calls @ref cc_bigint_lsh_u32.
void cc_bigint_lsh(size_t size, void* dst, const void* src);
/// @brief Bitwise left-shift
void cc_bigint_lsh_u32(size_t size, void* dst, uint32_t src);
/// @brief Bitwise right-shift. Calls @ref cc_bigint_rsh_u32.
void cc_bigint_rsh(size_t size, void* dst, const void* src);
/// @brief Bitwise right-shift
void cc_bigint_rsh_u32(size_t size, void* dst, uint32_t src);

// === Casting ===

/// @brief Sign-extend integer
/// @param dst The output. Can be the same as `src`, otherwise it must not overlapping.
/// @param src The input, to be extended and stored in `dst`
void cc_bigint_extend_sign(size_t dst_size, void* dst, size_t src_size, const void* src);
/// @brief Zero-extend integer
/// @param dst The output. Can be the same as `src`, otherwise it must not overlapping.
/// @param src The input, to be extended and stored in `dst`
void cc_bigint_extend_zero(size_t dst_size, void* dst, size_t src_size, const void* src);