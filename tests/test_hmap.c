#include "test.h"
#include <cc/lib.h>
#include <stdio.h>

int test_hmap(void)
{
    cc_hmap32 map;
    cc_hmap32_create(&map);

    const uint32_t lower_bound = 100;
    const uint32_t upper_bound = 1000;
    const uint32_t del_lower_bound = 300;
    const uint32_t del_upper_bound = 550;

    for (uint32_t i = lower_bound; i <= upper_bound; ++i)
    {
        bool replaced = cc_hmap32_put(&map, i, i);
        test_assert("Expected a unique key that does not replace any existing keys", !replaced);
    }
    
    for (uint32_t i = upper_bound + 1; i <= upper_bound + 1000; ++i)
    {
        uint32_t value;
        bool exists = cc_hmap32_get(&map, i, &value);
        test_assert("Expected key `i` to not exist", !exists);
    }

    size_t utilized = 0;
    for (size_t i = 0; i < map.cap_bucket; ++i)
    {
        uint8_t* flags = (uint8_t*)((uint32_t*)map.bucket + map.cap_bucket);
        if (flags[i])
            ++utilized;
    }

    printf("%zu/%zu (%.2f%%) bucket indices utilized\n", utilized, (size_t)map.num_entries, utilized * 100.f / map.num_entries);

    for (uint32_t i = del_lower_bound; i <= del_upper_bound; ++i)
    {
        bool deleted = cc_hmap32_delete(&map, i);
        test_assert("Expected key `i` to exist", deleted);
    }
    
    for (uint32_t i = lower_bound; i <= upper_bound; ++i)
    {
        uint32_t value;
        bool exists = cc_hmap32_get(&map, i, &value);
        bool should_exist = i < del_lower_bound || i > del_upper_bound;

        if (exists != should_exist)
        {
            if (exists)
                printf("Key was not deleted: %d\n", i);
            else
                printf("Key was wrongly deleted: %d\n", i);
        }
        test_assert("Expected `exists == should_exist`", exists == should_exist);

        if (exists)
        {
            if (value != i)
                printf("Incorrect mapping: %d -> %d\n", i, value);
            test_assert("Expected a mapping of i -> i", value == i);
        }
    }

    cc_hmap32_destroy(&map);

    return 1;
}