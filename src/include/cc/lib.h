#pragma once
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>

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
