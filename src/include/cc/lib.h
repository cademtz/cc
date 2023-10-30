#pragma once
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>
#include <stdbool.h>

/**
 * @file
 * @brief Configurable C libraries used throughout the project.
 * 
 * To use a wider string type, define the `CC_STR` macro function.
 * 
 * For example: `CC_STR(x) L##x` will turn all the string literals into wide-char.
 * Then redefine `cc_strlen`, `cc_strncmp`, etc... to use the correct wide-char string functions.
 */

#ifndef CC_STR
    #define cc_strlen strlen
    #define cc_isdigit isdigit
    #define cc_isspace isspace
    #define cc_isalpha isalpha
    #define cc_isalnum isalnum
    #define cc_strncmp strncmp

    typedef char cc_char;
    // Format a string literal to use the correct character type.
    // So, if `cc_char` is `wchar_t` then `CC_STR("text")` must become `L"text"`
    #define CC_STR(x) x
#endif

/// @brief Align `x` to the next multiple of `align`
#define CC_ALIGN(x, align) ((x % align) ? x - (x % align) + align : x)

typedef struct cc_arena
{
    size_t size;
    size_t capacity;
} cc_arena;

cc_arena* cc_arena_create(void);
static void cc_arena_destroy(cc_arena* a) { free(a); }
static char* cc_arena_dataptr(cc_arena* a) { return (char*)(a + 1); }
void cc_arena_resize(cc_arena** a, size_t size, int default_value);
void* cc_arena_alloc_align(cc_arena** a, size_t size, size_t align);
void* cc_arena_alloc(cc_arena** a, size_t size);

#define cc_arena_clear(a) cc_arena_resize(a, 0, 0)

/**
 * @brief Record heap allocations in order and free multiple of them at once.
 * 
 * This is mainly used by the parser, where every allocation
 * made after a certain period may have to be undone.
 */
typedef struct cc_heaprecord
{
    /// @brief A growing array of every allocated pointer
    void** allocs;
    /// @brief Number of pointers in @ref allocs
    size_t num_allocs;
    /// @brief Capacity of @ref allocs
    size_t cap_allocs;
} cc_heaprecord;

void cc_heaprecord_create(cc_heaprecord* record);
void cc_heaprecord_destroy(cc_heaprecord* record);
void* cc_heaprecord_alloc(cc_heaprecord* record, size_t size);
void cc_heaprecord_free(cc_heaprecord* record, void* alloc);
/// @brief Free the last `n` heap allocations
/// @param n Number of allocations to free, where `n <= num_allocs`
void cc_heaprecord_pop(cc_heaprecord* record, size_t n);

/// @brief Calculate the 32-bit FNV1-a hash
uint32_t cc_fnv1a_32(const void* data, size_t size);
uint32_t cc_fnv1a_u32(uint32_t i);
static uint32_t cc_fnv1a_i32(int32_t i) { return cc_fnv1a_u32((uint32_t)i); }

/**
 * @brief A data stream with the ability to read/write integers.
 * 
 * Endianness is stream-dependent.
 * The stream is responsible for converting integers to/from the desired endianness.
 */
typedef struct cc_stream
{
    /// @brief Destroy the current stream
    void(*destroy)(struct cc_stream* self);
    /**
     * @brief Write bytes until the buffer or stream ends
     * @param self The current stream
     * @param data Byte array
     * @param size Size of byte array
     * @return Number of bytes written
     */
    size_t(*write)(struct cc_stream* self, const uint8_t* data, size_t size);
    /**
     * @brief Read bytes until the buffer or stream ends
     * @param self The current stream
     * @param buffer Byte buffer
     * @param size Size of byte buffer
     * @return Number of bytes read
     */
    size_t(*read)(struct cc_stream* self, uint8_t* buffer, size_t size);
} cc_stream;

typedef struct cc_staticstream
{
    cc_stream base;
    uint8_t* buffer;
    size_t size;
    size_t readpos;
    size_t writepos;
} cc_staticstream;

typedef struct cc_dynamicstream
{
    cc_stream base;
    uint8_t* buffer;
    size_t size;
    size_t cap;
    size_t readpos;
    size_t writepos;
} cc_dynamicstream;

/// @brief Create a stream from a static buffer
cc_stream* cc_stream_create_static(uint8_t* buffer, size_t size);
/// @brief Create a growing stream
cc_stream* cc_stream_create_dynamic(void);
static void cc_stream_destroy(cc_stream* stream) { stream->destroy(stream); }
static size_t cc_stream_write(cc_stream* stream, const uint8_t* data, size_t size) {
    return stream->write(stream, data, size);
}
static size_t cc_stream_read(cc_stream* stream, uint8_t* buffer, size_t size) {
    return stream->read(stream, buffer, size);
}

enum cc_hmap_flag
{
    CC_HMAP_FLAG_EXISTS = 1 << 0,
};

/// @brief The bucket will grow when `cap_bucket < cap_entries * CC_HMAP_MINBUCKET`
#define CC_HMAP_MINBUCKET 1.2
/// @brief The bucket will resize to `cap_entries * CC_HMAP_MAXBUCKET`
#define CC_HMAP_MAXBUCKET 1.4
#define CC_HMAP_MINENTRIES 1.2

typedef struct cc_hmap32entry
{
    uint32_t key;
    uint32_t value;
    /// @brief The next entry with the same hash
    uint32_t next_index;
} cc_hmap32entry;

/**
 * @brief A hashmap with 32-bit integer keys and values.
 * 
 * A maximum of `UINT32_MAX - 1` entries can be held (which would require over 48 gigabytes).
 */
typedef struct cc_hmap32
{
    /**
     * @brief Two arrays taped together.
     * 
     * - A `uint32_t` index array where: `entry = entries[index_array[hash]]`
     * - A `uint8_t` flags array where: `flags = flags_array[hash]`
     * The `flags_array` is found after the index array.
     */
    void* bucket;
    /**
     * @brief Array containing all entries grouped by hash.
     * 
     * When a bucket points at a collision
     */
    cc_hmap32entry* entries;
    /// @brief Capacity of the `bucket` array
    size_t cap_bucket;
    /// @brief Number of items in `entries`
    uint32_t num_entries;
    /// @brief Item capacity of `entries`. This increments with each new entry does not shrink.
    uint32_t cap_entries;
} cc_hmap32;

/// @brief 0-initialize the map
static inline void cc_hmap32_create(cc_hmap32* map) {
    memset(map, 0, sizeof(*map));
}
/// @brief Free memory and 0-initialize the map for later use
void cc_hmap32_destroy(cc_hmap32* map);
/// @brief Empty all values, potentially making room for new ones
void cc_hmap32_clear(cc_hmap32* map);
/// @brief Get the number of entries in the map
static inline size_t cc_hmap32_size(const cc_hmap32* map) { return map->num_entries; }
/// @brief Put a new value or replace an old value
/// @return `true` when a value is replaced
bool cc_hmap32_put(cc_hmap32* map, uint32_t key, uint32_t value);
/// @brief Put a new value and return the old value (if any)
/// @param old_value Pointer to store the old value
/// @return `true` if a value is replaced and `old_value` is set.
bool cc_hmap32_swap(cc_hmap32* map, uint32_t key, uint32_t value, uint32_t* old_value);
/// @brief Put a new value
/// @return `true` if a value is replaced
bool cc_hmap32_put(cc_hmap32* map, uint32_t key, uint32_t value);
uint32_t cc_hmap32_get_default(const cc_hmap32* map, uint32_t key, uint32_t default_value);