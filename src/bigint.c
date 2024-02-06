#include <cc/bigint.h>
#include <string.h>

enum cc_bigint_endian cc_bigint_endianness()
{
    uint16_t value = 1;
    return *(uint8_t*)&value == 0 ? CC_BIGINT_ENDIAN_BIG : CC_BIGINT_ENDIAN_LITTLE;
}

void cc_bigint_i32(size_t size, void* dst, int32_t src)
{
    memset(dst, src < 0 ? 0xFF : 0, size);
    int32_t* low32 = (int32_t*)dst;
    if (cc_bigint_endianness() == CC_BIGINT_ENDIAN_BIG)
        low32 = (int32_t*)((uint8_t*)dst + size - 4);
    *low32 = src;
}
void cc_bigint_u32(size_t size, void* dst, uint32_t src)
{
    memset(dst, 0, size);
    uint32_t* low32 = (int32_t*)dst;
    if (cc_bigint_endianness() == CC_BIGINT_ENDIAN_BIG)
        low32 = (uint32_t*)((uint8_t*)dst + size - 4);
    *low32 = src;
}

static int cc__char_to_int(int radix, int ch, int* out_int)
{
    switch (radix)
    {
    case 2:
        if (ch < '0' || ch > '1')
            return 0;
        *out_int = ch - '0';
        return 1;
    case 8:
        if (ch < '0' || ch > '7')
            return 0;
        *out_int = ch - '0';
        return 1;
    case 10:
        if (ch < '0' || ch > '9')
            return 0;
        *out_int = ch - '0';
        return 1;
    case 16:
        ch = tolower(ch);
        if (ch >= '0' && ch <= '9')
            *out_int = ch - '0';
        else if (ch >= 'a' && ch <= 'f')
            *out_int = ch - 'a' + 0xA;
        else
            return 0;
        return 1;
    default:
        return 0;
    }
}

size_t cc_bigint_atoi(size_t size, void* dst, int radix, const char* str, size_t str_len)
{
    memset(dst, 0, size);

    if (str_len == (size_t)-1)
        str_len = strlen(str);
    
    size_t str_index = 0;
    int sign_bit = 0;
    if (str_index < str_len && str[str_index] == '-')
    {
        sign_bit = 1;
        ++str_index;
    }

    for (; str_index < str_len; ++str_index)
    {
        int char_to_int;
        if (!cc__char_to_int(radix, str[str_index], &char_to_int))
            return str_index;
        
        cc_bigint_umul_u32(size, dst, (int32_t)radix);
        cc_bigint_add_i32(size, dst, char_to_int);
    }

    if (sign_bit)
        cc_bigint_neg(size, dst);

    return str_index;
}

uint8_t cc_bigint_byte(size_t size, const void* src, size_t byte_index)
{
    if (cc_bigint_endianness() == CC_BIGINT_ENDIAN_BIG)
        byte_index = size - 1 - byte_index;
    return ((const uint8_t*)src)[byte_index];
}

const uint8_t* cc_bigint_byteptr_const(size_t size, const void* src, size_t byte_index)
{
    if (cc_bigint_endianness() == CC_BIGINT_ENDIAN_BIG)
        byte_index = size - 1 - byte_index;
    return (const uint8_t*)src + byte_index;
}

uint8_t* cc_bigint_byteptr(size_t size, void* src, size_t byte_index) {
    return (uint8_t*)cc_bigint_byteptr_const(size, src, byte_index);
}

uint8_t cc__bigint_add_u8_carry(uint8_t* dst, uint8_t src, uint8_t carry)
{
    uint16_t result = (uint16_t)*dst + (uint16_t)src + (uint16_t)carry;
    *dst = (uint8_t)result;
    return (uint8_t)(result >> 8);
}

void cc_bigint_add(size_t size, void* dst, const void* src)
{
    if (size == 1)
        *(uint8_t*)dst += *(const uint8_t*)src;
    else if (size == 2)
        *(uint16_t*)dst += *(const uint16_t*)src;
    else if (size == 4)
        *(uint32_t*)dst += *(const uint32_t*)src;
    else if (size == 8)
        *(uint64_t*)dst += *(const uint64_t*)src;
    else
    {
        uint8_t carry = 0;
        // Perform 1-byte additions
        for (size_t i = 0; i < size; ++i)
            carry = cc__bigint_add_u8_carry(cc_bigint_byteptr(size, dst, i), cc_bigint_byte(size, src, i), carry);
    }
}

void cc__bigint_add_32(size_t size, void* dst, uint32_t src, int sign_bit)
{
    if (size == 1)
        *(int8_t*)dst += (int8_t)src;
    else if (size == 2)
        *(int16_t*)dst += (int16_t)src;
    else if (size == 4)
        *(int32_t*)dst += (int32_t)src;
    else if (size == 8)
        *(int64_t*)dst += (int64_t)(int32_t)src;
    else
    {
        const int sign_extension = sign_bit ? -1 : 0;
        uint8_t carry = 0;
        
        // Perform 1-byte additions
        for (size_t i = 0; i < size; ++i)
        {
            uint8_t rhs = (uint8_t)sign_extension;
            if (i < sizeof(src))
                rhs = cc_bigint_byte(sizeof(src), &src, i);
            carry = cc__bigint_add_u8_carry(cc_bigint_byteptr(size, dst, i), rhs, carry);
        }
    }
}

/// @brief Multiply two bytes, shift the result according to their byte index, and add it to `dst`
static void cc__bigint_mul_byte(size_t size, void* dst, uint8_t lhs, size_t lhs_byte, uint8_t rhs, size_t rhs_byte)
{
    uint16_t result = (uint16_t)lhs * (uint16_t)rhs;
    size_t result_index = lhs_byte + rhs_byte;

    if (result == 0)
        return;
    if (result_index >= size)
        return; // Result is too large
    
    uint8_t carry = 0;

    // Add the 2-byte `result` using the sign-extension trick; We pretend to read 0s beyond the size of our result.
    for (size_t i = result_index; i < size; ++i)
    {
        uint8_t rhs = 0;
        if (i < result_index + sizeof(result))
            rhs = cc_bigint_byte(sizeof(result), &result, i - result_index);
        carry = cc__bigint_add_u8_carry(cc_bigint_byteptr(size, dst, i), rhs, carry);
    }
}

void cc_bigint_umul(size_t size, void* dst, const void* src)
{
    if (size == 1)
        *(uint8_t*)dst *= *(const uint8_t*)src;
    else if (size == 2)
        *(uint16_t*)dst *= *(const uint16_t*)src;
    else if (size == 4)
        *(uint32_t*)dst *= *(const uint32_t*)src;
    else if (size == 8)
        *(uint64_t*)dst *= *(const uint64_t*)src;
    else
    {
        // Perform 1-byte multiplications
        // We must multiply the most-significant bytes first, so the iteration is backwards while preventing size_t underflow
        for (size_t lhs_iter = 0; lhs_iter < size; ++lhs_iter)
        {
            size_t lhs_index = size - 1 - lhs_iter;
            uint8_t* lhs_byteptr = cc_bigint_byteptr(size, dst, lhs_index);
            uint8_t lhs_byte = *lhs_byteptr;
            *lhs_byteptr = 0;
            
            for (size_t rhs_iter = 0; rhs_iter < size; ++rhs_iter)
            {
                size_t rhs_index = size - 1 - rhs_iter;
                uint8_t rhs_byte = cc_bigint_byte(size, src, rhs_index);
                cc__bigint_mul_byte(size, dst, lhs_byte, lhs_index, rhs_byte, rhs_index);
            }
        }
    }
}

void cc_bigint_umul_u32(size_t size, void* dst, uint32_t src)
{
    if (size == 1)
        *(uint8_t*)dst *= (uint8_t)src;
    else if (size == 2)
        *(uint16_t*)dst *= (uint16_t)src;
    else if (size == 4)
        *(uint32_t*)dst *= (uint32_t)src;
    else if (size == 8)
        *(uint64_t*)dst *= (uint64_t)src;
    else
    {
        // Perform 1-byte multiplications
        // We must multiply the most-significant bytes first, so the iteration is backwards while preventing size_t underflow
        for (size_t lhs_iter = 0; lhs_iter < size; ++lhs_iter)
        {
            size_t lhs_index = size - 1 - lhs_iter;
            uint8_t* lhs_byteptr = cc_bigint_byteptr(size, dst, lhs_index);
            uint8_t lhs_byte = *lhs_byteptr;
            *lhs_byteptr = 0;
            
            for (size_t rhs_iter = 0; rhs_iter < sizeof(src); ++rhs_iter)
            {
                size_t rhs_index = sizeof(src) - 1 - rhs_iter;
                uint8_t rhs_byte = cc_bigint_byte(size, &src, rhs_index);
                cc__bigint_mul_byte(size, dst, lhs_byte, lhs_index, rhs_byte, rhs_index);
            }
        }
    }
}

void cc_bigint_neg(size_t size, void* dst)
{
    cc_bigint_not(size, dst);
    cc_bigint_add_u32(size, dst, 1);
}

void cc_bigint_not(size_t size, void* dst)
{
    for (size_t i = 0; i < size; ++i)
        *((uint8_t*)dst + i) = ~*((uint8_t*)dst + i);
}

int cc_bigint_sign(size_t size, const void* lhs) {
    return cc_bigint_byte(size, lhs, size - 1) >> 7;
}

int cc_bigint_cmp(size_t size, const void* lhs, const void* rhs)
{
    int diff = cc_bigint_sign(size, rhs) - cc_bigint_sign(size, lhs);
    if (diff)
        return diff;
    return cc_bigint_ucmp(size, lhs, rhs);
}

int cc_bigint_ucmp(size_t size, const void* lhs, const void* rhs)
{
    // Compare most-significant bytes first
    for (size_t i = 0; i < size; ++i)
    {
        int16_t diff = (uint16_t)cc_bigint_byte(size, lhs, size - 1 - i) - cc_bigint_byte(size, rhs, size - 1 - i);
        if (diff > 0)
            return 1;
        else if (diff < 0)
            return -1;
    }
    return 0;
}