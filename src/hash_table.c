// Simple hash table implemented in C.
// From: https://github.com/benhoyt/ht

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hash_table.h"

#define INITIAL_CAPACITY 16

// Tombstone sentinel: a key whose str pointer is this private address.
static char   _tombstone_buf[] = "";
static ht_key TOMBSTONE        = { .type = HT_KEY_STR,
                                   .str  = (const char*)&_tombstone_buf };

#define IS_TOMBSTONE(e) \
    ((e).key.type == HT_KEY_STR && (e).key.str == TOMBSTONE.str)
#define IS_EMPTY(e) \
    ((e).key.type == HT_KEY_STR && (e).key.str == NULL)

// ---------------------------------------------------------------------------
// Hash functions (both private)
// ---------------------------------------------------------------------------

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME  1099511628211UL

static uint64_t hash_str(const char* key) {
    uint64_t hash = FNV_OFFSET;
    for (const char* p = key; *p; p++) {
        hash ^= (uint64_t)(unsigned char)(*p);
        hash *= FNV_PRIME;
    }
    return hash;
}

static uint64_t hash_int(intptr_t key) {
    uint64_t x = (uint64_t)key;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9UL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebUL;
    return x ^ (x >> 31);
}

static uint64_t hash_key(ht_key key) {
    return key.type == HT_KEY_STR ? hash_str(key.str) : hash_int(key.i);
}

// ---------------------------------------------------------------------------
// Key equality
// ---------------------------------------------------------------------------

static bool key_equal(ht_key a, ht_key b) {
    if (a.type != b.type) return false;
    if (a.type == HT_KEY_STR) return strcmp(a.str, b.str) == 0;
    return a.i == b.i;
}

// ---------------------------------------------------------------------------
// Create / destroy
// ---------------------------------------------------------------------------

ht* ht_create(void) {
    ht* table = malloc(sizeof(ht));
    if (table == NULL) return NULL;

    table->length   = 0;
    table->capacity = INITIAL_CAPACITY;
    table->entries  = calloc(table->capacity, sizeof(ht_entry));
    if (table->entries == NULL) {
        free(table);
        return NULL;
    }
    return table;
}

void ht_destroy(ht* table) {
    for (size_t i = 0; i < table->capacity; i++) {
        ht_entry* e = &table->entries[i];
        if (!IS_EMPTY(*e) && !IS_TOMBSTONE(*e) && e->key.type == HT_KEY_STR) {
            free((void*)e->key.str);
        }
    }
    free(table->entries);
    free(table);
}

// ---------------------------------------------------------------------------
// Internal: find slot
// ---------------------------------------------------------------------------

// Returns the index of the slot for key: either the existing entry or the
// first empty/tombstone slot where the key should be inserted.
static size_t find_slot(ht_entry* entries, size_t capacity, ht_key key) {
    uint64_t hash      = hash_key(key);
    size_t   index     = (size_t)(hash & (uint64_t)(capacity - 1));
    size_t   tombstone = SIZE_MAX;  // index of first tombstone seen

    while (true) {
        ht_entry* e = &entries[index];

        if (IS_EMPTY(*e)) {
            // Prefer reusing a tombstone slot over an empty one.
            return tombstone != SIZE_MAX ? tombstone : index;
        }

        if (IS_TOMBSTONE(*e)) {
            if (tombstone == SIZE_MAX) tombstone = index;
        } else if (key_equal(e->key, key)) {
            return index;  // found existing key
        }

        if (++index >= capacity) index = 0;
    }
}

// ---------------------------------------------------------------------------
// Internal: expand
// ---------------------------------------------------------------------------

static bool ht_expand(ht* table) {
    size_t new_capacity = table->capacity * 2;
    if (new_capacity < table->capacity) return false;  // overflow

    ht_entry* new_entries = calloc(new_capacity, sizeof(ht_entry));
    if (new_entries == NULL) return false;

    // Rehash all live entries into the new array.
    for (size_t i = 0; i < table->capacity; i++) {
        ht_entry* e = &table->entries[i];
        if (IS_EMPTY(*e) || IS_TOMBSTONE(*e)) continue;
        size_t new_index = find_slot(new_entries, new_capacity, e->key);
        new_entries[new_index] = *e;
    }

    free(table->entries);
    table->entries  = new_entries;
    table->capacity = new_capacity;
    return true;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void* ht_get(ht* table, ht_key key) {
    size_t    index = find_slot(table->entries, table->capacity, key);
    ht_entry* e     = &table->entries[index];
    if (IS_EMPTY(*e) || IS_TOMBSTONE(*e)) return NULL;
    return e->value;
}

ht_key ht_set(ht* table, ht_key key, void* value) {
    assert(value != NULL);
    static ht_key null_key = { .type = HT_KEY_STR, .str = NULL };
    if (value == NULL) return null_key;

    if (table->length >= table->capacity / 2) {
        if (!ht_expand(table)) return null_key;
    }

    size_t    index = find_slot(table->entries, table->capacity, key);
    ht_entry* e     = &table->entries[index];

    if (!IS_EMPTY(*e) && !IS_TOMBSTONE(*e)) {
        // Existing key: update value only.
        e->value = value;
        return e->key;
    }

    // New entry: copy string keys onto the heap; integer keys need no copy.
    if (key.type == HT_KEY_STR) {
        char* key_copy = strdup(key.str);
        if (key_copy == NULL) return null_key;
        key.str = key_copy;
    }

    e->key   = key;
    e->value = value;
    table->length++;
    return e->key;
}

void* ht_remove(ht* table, ht_key key) {
    size_t    index = find_slot(table->entries, table->capacity, key);
    ht_entry* e     = &table->entries[index];

    if (IS_EMPTY(*e) || IS_TOMBSTONE(*e)) return NULL;

    void* old_value = e->value;

    if (e->key.type == HT_KEY_STR) free((void*)e->key.str);
    e->key   = TOMBSTONE;
    e->value = NULL;
    table->length--;
    return old_value;
}

size_t ht_length(ht* table) {
    return table->length;
}

// ---------------------------------------------------------------------------
// Iterator
// ---------------------------------------------------------------------------

hti ht_iterator(ht* table) {
    hti it;
    it._table = table;
    it._index = 0;
    it.value  = NULL;
    return it;
}

bool ht_next(hti* it) {
    ht* table = it->_table;
    while (it->_index < table->capacity) {
        size_t    i = it->_index++;
        ht_entry* e = &table->entries[i];
        if (!IS_EMPTY(*e) && !IS_TOMBSTONE(*e)) {
            it->key   = e->key;
            it->value = e->value;
            return true;
        }
    }
    return false;
}