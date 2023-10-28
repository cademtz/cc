#include "test.h"
#include <cc/lib.h>
#include <stdio.h>

int test_hmap(void)
{
    cc_hmap32 map;
    cc_hmap32_create(&map);

    const uint32_t lower_bound = 100;
    const uint32_t upper_bound = 10000;

    for (uint32_t i = lower_bound; i <= upper_bound; ++i)
    {
        bool replaced = cc_hmap32_put(&map, i, i);
        test_assert("Expected a unique key that does not replace any existing keys", !replaced);
    }
    
    size_t utilized = 0;
    for (size_t i = 0; i < map.cap_bucket; ++i)
    {
        uint8_t* flags = (uint8_t*)((uint32_t*)map.bucket + map.cap_bucket);
        if (flags[i])
            ++utilized;
    }

    printf("%zu/%zu (%.2f%%) bucket indices utilized\n", utilized, map.num_entries, utilized * 100.f / map.num_entries);
    
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