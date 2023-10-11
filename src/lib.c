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

uint32_t cc_fnv1a_32(const void* data, size_t size)
{
    const uint32_t FNV_PRIME = 0x1000193;
    const uint32_t FNV_OFFSET = 0x811c9dc5;

    uint32_t hash = FNV_OFFSET;
    for (size_t i = 0; i < size; ++i)
    {
        hash ^= ((const uint8_t*)data)[i];
        hash *= FNV_PRIME;
    }
    return hash;
}
uint32_t cc_fnv1a_u32(uint32_t i) {
    return cc_fnv1a_32(&i, sizeof(i));
}

cc_staticstream* cc_staticstream_create(uint8_t* buffer, size_t size);
void cc_staticstream_destroy(cc_stream* stream);
size_t cc_staticstream_read(cc_stream* self, uint8_t* buffer, size_t size);
size_t cc_staticstream_write(cc_stream* self, const uint8_t* data, size_t size);

cc_dynamicstream* cc_dynamicstream_create(void);
void cc_dynamicstream_destroy(cc_stream* stream);
size_t cc_dynamicstream_read(cc_stream* self, uint8_t* buffer, size_t size);
size_t cc_dynamicstream_write(cc_stream* self, const uint8_t* data, size_t size);

cc_stream* cc_stream_create_static(uint8_t* buffer, size_t size) {
    return &cc_staticstream_create(buffer, size)->base;
}
cc_stream* cc_stream_create_dynamic(void) {
    return &cc_dynamicstream_create()->base;
}

cc_staticstream* cc_staticstream_create(uint8_t* buffer, size_t size)
{
    cc_staticstream* stream = (cc_staticstream*)malloc(sizeof(*stream));
    memset(stream, 0, sizeof(*stream));
    stream->buffer = buffer;
    stream->size = size;
    stream->base.destroy = &cc_staticstream_destroy;
    stream->base.read = &cc_staticstream_read;
    stream->base.write = &cc_staticstream_write;
    return stream;
}
void cc_staticstream_destroy(cc_stream* stream) { free(stream); }
size_t cc_staticstream_read(cc_stream* self, uint8_t* buffer, size_t size)
{
    cc_staticstream* s = (cc_staticstream*)self;
    size_t limit = s->size - s->readpos;
    if (size > limit)
        size = limit;
    memcpy(buffer, s->buffer + s->readpos, size);
    s->readpos += size;
    return size;
}
size_t cc_staticstream_write(cc_stream* self, const uint8_t* data, size_t size)
{
    cc_staticstream* s = (cc_staticstream*)self;
    size_t limit = s->size - s->writepos;
    if (size > limit)
        size = limit;
    memcpy(s->buffer + s->writepos, data, size);
    s->writepos += size;
    return size;
}

cc_dynamicstream* cc_dynamicstream_create(void)
{
    cc_dynamicstream* stream = (cc_dynamicstream*)malloc(sizeof(*stream));
    stream->base.destroy = &cc_dynamicstream_destroy;
    stream->base.read = &cc_dynamicstream_read;
    stream->base.write = &cc_dynamicstream_write;
    return stream;
}
void cc_dynamicstream_destroy(cc_stream* self)
{
    cc_dynamicstream* s = (cc_dynamicstream*)self;
    free(s->buffer);
    free(s);
}
size_t cc_dynamicstream_read(cc_stream* self, uint8_t* buffer, size_t size)
{
    cc_dynamicstream* s = (cc_dynamicstream*)self;
    size_t limit = s->size - s->readpos;
    if (size > limit)
        size = limit;
    memcpy(buffer, s->buffer + s->readpos, size);
    s->readpos += size;
    return size;
}
size_t cc_dynamicstream_write(cc_stream* self, const uint8_t* data, size_t size)
{
    cc_dynamicstream* s = (cc_dynamicstream*)self;
    size_t limit = s->size - s->writepos;
    if (size > limit)
    {
        s->size += size;
        if (s->size > s->cap)
        {
            s->cap = s->size;
            s->buffer = (uint8_t*)realloc(s->buffer, s->cap);
        }
    }
    memcpy(s->buffer + s->writepos, data, size);
    s->writepos += size;
    return size;
}