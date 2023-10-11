#pragma once
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>
#include <stdbool.h>

#ifndef CC_LIB
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