#include "test.h"
#include <cc/bigint.h>
#include <stdio.h>
#include <string.h>

static void print_bigint(size_t size, const void* src)
{
    for (size_t i = 0; i < size; ++i)
        printf("%.2X, ", *((uint8_t*)src + i));
}

int test_bigint(void)
{
    printf("Endianness: %s\n", cc_bigint_endianness() == CC_BIGINT_ENDIAN_LITTLE ? "Little-endian" : "Big-endian");

    // Test string-to-int conversion
    {
        uint8_t lhs[16], answer[16];
        cc_bigint_atoi(sizeof(lhs), lhs, 16, "-11223344", -1);
        cc_bigint_i32(sizeof(answer), answer, -0x11223344);

        printf("\"-11223344\" -> ");
        print_bigint(sizeof(lhs), lhs);
        printf("\n");

        test_assert("Expected -0x11223344 sign-extended to a 16-byte int", !memcmp(lhs, answer, sizeof(lhs)));
    }
    // Test addition, subtraction, and comparison
    {
        uint8_t lhs[16], rhs[16], answer[16];
        cc_bigint_u32(sizeof(lhs), lhs, 0xAABBCCDD);
        cc_bigint_u32(sizeof(rhs), rhs, 0xBBCCDDAA);
        cc_bigint_atoi(sizeof(answer), answer, 16, "16688aa87", -1);

        cc_bigint_add(sizeof(lhs), lhs, rhs);

        printf("Addition:\n");
        printf("0xAABBCCDD + 0xBBCCDDAA: ");
        print_bigint(sizeof(lhs), lhs);
        printf("\n");

        test_assert("Expected 0xAABBCCDD + 0xBBCCDDAA == 0x16688AA87", !memcmp(lhs, answer, sizeof(lhs)));

        int comparsion = cc_bigint_cmp(sizeof(lhs), lhs, rhs);
        if (!comparsion)
            printf("lhs == rhs\n");
        else
            printf("lhs %c rhs\n", comparsion == 1 ? '>' : '<');
        
        test_assert("Expected lhs > rhs", comparsion == 1);
        
        cc_bigint_u32(sizeof(lhs), lhs, 0xAABBCCDD);
        cc_bigint_u32(sizeof(rhs), rhs, 0xBBCCDDAA);
        cc_bigint_atoi(sizeof(answer), answer, 10, "-286331085", -1);

        cc_bigint_sub(sizeof(lhs), lhs, rhs);

        test_assert("Expected 0xAABBCCDD - 0xBBCCDDAA == -286331085", !memcmp(lhs, answer, sizeof(lhs)));
    }
    // Test multiplication and divsion
    {
        uint8_t lhs[16], rhs[16], answer[16];
        
        // Unsigned multiplication

        cc_bigint_u32(sizeof(lhs), lhs, 0xAABBCCDD);
        cc_bigint_u32(sizeof(rhs), rhs, 0xAABBCCDD);
        cc_bigint_atoi(sizeof(answer), answer, 16, "4bf0ed84d3569e21c8263785", -1);

        cc_bigint_umul(sizeof(lhs), lhs, rhs);
        cc_bigint_umul(sizeof(lhs), lhs, rhs);

        printf("pow(0xAABBCCDD, 3): ");
        print_bigint(sizeof(lhs), lhs);
        printf("\n");

        test_assert("Expected pow(0xAABBCCDD, 3) in a 16-byte int", !memcmp(lhs, answer, sizeof(lhs)));

        // Signed multiplication
        cc_bigint_i32(sizeof(lhs), lhs, -0x11223344);
        cc_bigint_i32(sizeof(rhs), rhs, -0x11223344);
        cc_bigint_atoi(sizeof(answer), answer, 16, "-13a5bd40175ac840dc5c40", -1);

        cc_bigint_mul(sizeof(lhs), lhs, rhs);
        cc_bigint_i32(sizeof(rhs), rhs, -0x11223344);
        cc_bigint_mul(sizeof(lhs), lhs, rhs);
        
        test_assert("Expected pow(-0x11223344, 3) == -0x13a5bd40175ac840dc5c40", !memcmp(lhs, answer, sizeof(lhs)));

        uint8_t quotient[sizeof(lhs)], remainder[sizeof(rhs)];
        cc_bigint_atoi(sizeof(lhs), lhs, 16, "0000b3e446aa4414182370c311beafa9", -1);
        cc_bigint_atoi(sizeof(rhs), rhs, 16, "000000000043cc60cc51fd5902cd42d9", -1);
        cc_bigint_atoi(sizeof(answer), answer, 16, "2a740ed", -1);
        
        cc_bigint_udiv(sizeof(lhs), lhs, rhs, quotient, remainder);
        print_bigint(sizeof(lhs), lhs);
        printf("\n");

        test_assert("Expected the division of two random numbers", !memcmp(quotient, answer, sizeof(quotient)));
    }
    // Test basic bit operations
    {
        uint8_t rand0[16], rand1[16], answer[16];
        uint8_t lhs[16], rhs[16];
        cc_bigint_atoi(sizeof(rand0), rand0, 16, "9653299cca49c5347e81f89e09027d72", -1);
        cc_bigint_atoi(sizeof(rand1), rand1, 16, "c6467992eff88e4adc4cb4da60e583b8", -1);
        memcpy(rhs, rand1, sizeof(rhs));

        memcpy(lhs, rand0, sizeof(lhs));
        cc_bigint_and(sizeof(lhs), lhs, rhs);
        cc_bigint_atoi(sizeof(answer), answer, 16, "86422990ca4884005c00b09a00000130", -1);
        test_assert("Expected the bitwise-and of two random numbers", !memcmp(lhs, answer, sizeof(lhs)));
        
        memcpy(lhs, rand0, sizeof(lhs));
        cc_bigint_or(sizeof(lhs), lhs, rhs);
        cc_bigint_atoi(sizeof(answer), answer, 16, "d657799eeff9cf7efecdfcde69e7fffa", -1);
        test_assert("Expected the bitwise-or of two random numbers", !memcmp(lhs, answer, sizeof(lhs)));
        
        memcpy(lhs, rand0, sizeof(lhs));
        cc_bigint_xor(sizeof(lhs), lhs, rhs);
        cc_bigint_atoi(sizeof(answer), answer, 16, "5015500e25b14b7ea2cd4c4469e7feca", -1);
        test_assert("Expected the bitwise-xor of two random numbers", !memcmp(lhs, answer, sizeof(lhs)));
    }
    // Test bit shifts
    {
        uint8_t rand[16], answer[16];
        uint8_t lhs[16], rhs[16];
        cc_bigint_atoi(sizeof(rand), rand, 16, "116955a1e6bc5ae912f87be0cc88af50", -1);
        cc_bigint_u32(sizeof(rhs), rhs, 70);

        memcpy(lhs, rand, sizeof(lhs));
        cc_bigint_lsh(sizeof(lhs), lhs, rhs);
        cc_bigint_atoi(sizeof(answer), answer, 16, "be1ef833222bd4000000000000000000", -1);
        test_assert("Expected lhs to be shifted left 70 bits", !memcmp(lhs, answer, sizeof(lhs)));

        memcpy(lhs, rand, sizeof(lhs));
        cc_bigint_rsh(sizeof(lhs), lhs, rhs);
        cc_bigint_atoi(sizeof(answer), answer, 16, "45a556879af16b", -1);
        test_assert("Expected lhs to be shifted right 70 bits", !memcmp(lhs, answer, sizeof(lhs)));
    }

    return 1;
}