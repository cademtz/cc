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
    uint8_t bytes[4];
    bytes[3] = i & 0xFF;
    bytes[2] = (i >> 8) & 0xFF;
    bytes[1] = (i >> 16) & 0xFF;
    bytes[0] = (i >> 24) & 0xFF;
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

static inline uint32_t* cc_hmap32_indices(const cc_hmap32* map) { return (uint32_t*)map->bucket; }
static inline uint8_t* cc_hmap32_flags(const cc_hmap32* map) {
    return (uint8_t*)(cc_hmap32_indices(map) + map->cap_bucket);
}
static inline uint32_t cc_hmap32_hash(const cc_hmap32* map, uint32_t key) {
    return (map->cap_bucket == 0) ? 0 : cc_fnv1a_u32(key) % (uint32_t)map->cap_bucket;
}
/// @brief Append an entry to the `map->entries` vector
static cc_hmap32entry* cc_hmap32_append(cc_hmap32* map, uint32_t key, uint32_t value)
{
    ++map->num_entries;
    if (map->num_entries >= map->cap_entries)
    {
        map->cap_entries = map->num_entries;
        map->entries = (cc_hmap32entry*)realloc(map->entries, map->cap_entries * sizeof(map->entries[0]));
    }
    
    cc_hmap32entry* entry = &map->entries[map->num_entries - 1];
    memset(entry, 0, sizeof(entry));
    entry->key = key;
    entry->value = value;
    entry->next_index = UINT32_MAX;
    return entry;
}

/// @brief Resize the map's bucket to `map->cap_entries * CC_HMAP_MAXBUCKET`
static void cc_hmap32_grow(cc_hmap32* map)
{
    map->cap_bucket = (size_t)(map->cap_entries * CC_HMAP_MAXBUCKET) + 13000;
    size_t new_cap_bytes = map->cap_bucket * (sizeof(uint32_t) + sizeof(uint8_t));
    map->bucket = realloc(map->bucket, new_cap_bytes);

    // 0-initialize the flags array (indicates all bucket indexes are empty)
    uint32_t* indices = cc_hmap32_indices(map);
    uint8_t* flags_array = cc_hmap32_flags(map);
    memset(flags_array, 0, map->cap_bucket);

    // Re-add all entries to the bucket
    for (uint32_t i = 0; i < map->num_entries; ++i)
    {
        uint32_t hash = cc_hmap32_hash(map, map->entries[i].key);
        uint8_t* flags = &flags_array[hash];
        uint32_t* index = &indices[hash];
        cc_hmap32entry* entry = &map->entries[i];

        entry->next_index = UINT32_MAX;
        if (*flags & CC_HMAP_FLAG_EXISTS)
            entry->next_index = *index;
        *index = i;
        *flags |= CC_HMAP_FLAG_EXISTS;
    }
}

void cc_hmap32_destroy(cc_hmap32* map)
{
    free(map->bucket);
    free(map->entries);
}

void cc_hmap32_clear(cc_hmap32* map)
{
    uint8_t* flags = cc_hmap32_flags(map);
    memset(flags, 0, map->cap_bucket * sizeof(flags[0]));
    map->num_entries = 0;
}

bool cc_hmap32_swap(cc_hmap32* map, uint32_t key, uint32_t value, uint32_t* old_value)
{
    if (map->cap_bucket < (size_t)(map->cap_entries * CC_HMAP_MINBUCKET) + 1)
        cc_hmap32_grow(map);
    
    uint32_t hash = cc_hmap32_hash(map, key);
    uint8_t* flags = &cc_hmap32_flags(map)[hash];
    uint32_t* index = &cc_hmap32_indices(map)[hash];

    if ((*flags & CC_HMAP_FLAG_EXISTS) == 0)
        *index = map->num_entries;
    else
    {
        // Our key *might* be in list. Check colliding entries.
        cc_hmap32entry* next_entry;
        uint32_t next_index = *index;
        do
        {
            next_entry = &map->entries[next_index];
            if (next_entry->key == key)
            {
                next_entry->value = value;
                return true;
            }
            next_index = next_entry->next_index;
        } while (next_index != UINT32_MAX);
        
        // Our key was not in the list, but it does collide
        next_entry->next_index = map->num_entries;
    }

    *flags |= CC_HMAP_FLAG_EXISTS;
    cc_hmap32_append(map, key, value);
    return false;
}

bool cc_hmap32_put(cc_hmap32* map, uint32_t key, uint32_t value)
{
    uint32_t old_value;
    return cc_hmap32_swap(map, key, value, &old_value);
}

uint32_t cc_hmap32_get_default(const cc_hmap32* map, uint32_t key, uint32_t default_value)
{
    uint32_t hash = cc_hmap32_hash(map, key);
    uint8_t flags = cc_hmap32_flags(map)[hash];
    uint32_t index = cc_hmap32_indices(map)[hash];
    
    if ((flags & CC_HMAP_FLAG_EXISTS) == 0)
        return default_value;
    
    uint32_t next_index = index;
    do
    {
        const cc_hmap32entry* next_entry = &map->entries[next_index];
        if (next_entry->key == key)
            return next_entry->value;
        next_index = next_entry->next_index;
    } while (next_index != UINT32_MAX);
    
    return default_value;
}