// Simple hash table implemented in C.
// From: https://github.com/benhoyt/ht

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Key type
// ---------------------------------------------------------------------------

typedef struct {
    enum { HT_KEY_STR, HT_KEY_INT } type;
    union {
        const char* str;
        intptr_t    i;
    };
} ht_key;

// Convenience macros for constructing keys.
#define HT_STR(s) ((ht_key){ .type = HT_KEY_STR, .str = (s) })
#define HT_INT(n) ((ht_key){ .type = HT_KEY_INT, .i   = (n) })

// ---------------------------------------------------------------------------
// Hash table
// ---------------------------------------------------------------------------

typedef struct ht ht;

typedef struct {
    ht_key key;
    void*  value;  // NULL if slot is empty or a tombstone
} ht_entry;

struct ht {
    ht_entry* entries;
    size_t    capacity;
    size_t    length;
};

// Create hash table and return pointer to it, or NULL if out of memory.
ht* ht_create(void);

// Free memory allocated for hash table, including allocated string keys.
void ht_destroy(ht* table);

// Get item with given key. Returns value or NULL if not found.
void* ht_get(ht* table, ht_key key);

// Set item with given key. Value must not be NULL.
// For string keys, the key is copied into the table.
// Returns the stored key on success, or a key with a NULL str if out of memory.
ht_key ht_set(ht* table, ht_key key, void* value);

// Remove item with given key.
// Returns old value on success, NULL if not found.
void* ht_remove(ht* table, ht_key key);

// Return number of live items in hash table.
size_t ht_length(ht* table);

// ---------------------------------------------------------------------------
// Iterator
// ---------------------------------------------------------------------------

typedef struct {
    ht_key key;    // current key
    void*  value;  // current value

    // Don't use these fields directly.
    ht*    _table;
    size_t _index;
} hti;

// Return new hash table iterator (for use with ht_next).
hti ht_iterator(ht* table);

// Advance iterator to the next item, updating key and value.
// Returns true if an item was found, false when iteration is complete.
// Do not call ht_set or ht_remove during iteration.
bool ht_next(hti* it);