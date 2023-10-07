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

void cc_heaprecord_create(cc_heaprecord* record) {
    memset(record, 0, sizeof(*record));
}
void cc_heaprecord_destroy(cc_heaprecord* record)
{
    for (size_t i = 0; i < record->num_allocs; ++i)
        free(record->allocs[i]);
    free(record->allocs);
    memset(record, 0, sizeof(*record));
}
void* cc_heaprecord_alloc(cc_heaprecord* record, size_t size)
{
    ++record->num_allocs;
    if (record->num_allocs > record->cap_allocs)
    {
        ++record->cap_allocs;
        record->allocs = realloc(record->allocs, record->cap_allocs * sizeof(record->allocs[0]));
    }

    void* alloc = malloc(size);
    record->allocs[record->num_allocs - 1] = alloc;
    return alloc;
}
void cc_heaprecord_free(cc_heaprecord* record, void* alloc)
{
    free(alloc);
    // Find its array index and move the above items downwards
    for (size_t i = 0; i < record->num_allocs - 1; ++i)
    {
        if (record->allocs[i] == alloc)
        {
            size_t next = i + 1;
            memmove(record->allocs + i, record->allocs + next, (record->num_allocs - next) * sizeof(record->allocs[0]));
            break;
        }
    }
    --record->num_allocs;
}
void cc_heaprecord_pop(cc_heaprecord* record, size_t n)
{
    for (size_t i = record->num_allocs - n; i < record->num_allocs; ++i)
        free(record->allocs[i]);
    record->num_allocs -= n;
}