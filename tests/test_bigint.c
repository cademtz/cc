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

        test_assert("lhs must contain the same data as the answer", !memcmp(lhs, answer, sizeof(lhs)));
    }
    // Test addition and comparison
    {
        uint8_t lhs[16], rhs[16], answer[16];
        cc_bigint_u32(sizeof(lhs), lhs, 0xAABBCCDD);
        cc_bigint_u32(sizeof(rhs), rhs, 0xBBCCDDAA);
        cc_bigint_atoi(sizeof(answer), answer, 16, "16688aa87", -1);

        cc_bigint_add(sizeof(lhs), lhs, rhs);

        printf("Addition:\n");
        printf("0x0xAABBCCDD + 0xBBCCDDAA: ");
        print_bigint(sizeof(lhs), lhs);
        printf("\n");

        test_assert("lhs must contain the same data as the answer", !memcmp(lhs, answer, sizeof(lhs)));

        int comparsion = cc_bigint_cmp(sizeof(lhs), lhs, rhs);
        if (!comparsion)
            printf("lhs == rhs\n");
        else
            printf("lhs %c rhs\n", comparsion == 1 ? '>' : '<');
        
        test_assert("lhs must be greater than rhs", comparsion == 1);
    }
    // Test multiplication
    {
        uint8_t lhs[16], rhs[16], answer[16];
        cc_bigint_u32(sizeof(lhs), lhs, 0xAABBCCDD);
        cc_bigint_u32(sizeof(rhs), rhs, 0xAABBCCDD);
        cc_bigint_atoi(sizeof(answer), answer, 16, "4bf0ed84d3569e21c8263785", -1);

        cc_bigint_umul(sizeof(lhs), lhs, rhs);
        cc_bigint_umul(sizeof(lhs), lhs, rhs);

        printf("pow(0xAABBCCDD, 3): ");
        print_bigint(sizeof(lhs), lhs);
        printf("\n");

        test_assert("lhs must contain the same data as the answer", !memcmp(lhs, answer, sizeof(lhs)));
    }

    return 1;
}