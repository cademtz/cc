#include "test.h"
#include <cc/lib.h>

int test_hmap(void)
{
    cc_hmap32 map;
    cc_hmap32_create(&map);

    const uint32_t lower_bound = 100;
    const uint32_t upper_bound = 10000;

    for (uint32_t i = lower_bound; i <= upper_bound; ++i)
        cc_hmap32_put(&map, i, i);
    
    for (uint32_t i = lower_bound; i <= upper_bound; ++i)
    {
        uint32_t value = cc_hmap32_get_default(&map, i, 0);
        if (value == 0)
            printf("Missing key: %d\n", i);
        else if (value != i)
            printf("Incorrect mapping: %d -> %d\n", i, value);
        test_assert("Expected key `i` to exist", value != 0);
        test_assert("Expected a mapping of i -> i", value == i);
    }

    cc_hmap32_destroy(&map);

    return 1;
}