#include <cc/lib.h>

cc_arena* cc_arena_create(void)
{
    cc_arena* a = (cc_arena*)malloc(sizeof(*a));
    memset(a, 0, sizeof(*a));
    return a;
}

void cc_arena_resize(cc_arena** arena, size_t size, int default_value)
{
    cc_arena* a = *arena;
    if (size > a->capacity) {
        size_t new_cap = sizeof(*a) + size;
        a = realloc(a, new_cap);
        a->capacity = new_cap;
    }

    size_t old_size = a->size;
    if (size > old_size)
        memset(cc_arena_dataptr(a) + old_size, default_value, size - old_size);
    a->size = size;
    
    *arena = a;
}

void* cc_arena_alloc_align(cc_arena** arena, size_t size, size_t align)
{
    cc_arena* a = *arena;

    size_t padding = align - (a->size % align);
    if (padding == align)
        padding = 0;
    cc_arena_resize(&a, a->size + padding + size, 0);

    *arena = a;
    return cc_arena_dataptr(a) + a->size - size;
}

void* cc_arena_alloc(cc_arena** arena, size_t size)
{
    size_t align = 1;
    if (size == sizeof(short)) align = sizeof(short);
    else if (size == sizeof(int)) align = sizeof(int);
    else if (size == sizeof(long)) align = sizeof(long);
    else if (size == sizeof(void*)) align = sizeof(void*);
    return cc_arena_alloc_align(arena, size, align);
}