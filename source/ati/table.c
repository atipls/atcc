#include "table.h"

#define LOAD_CAPACITY_FACTOR 0.75

static u64 string_hash(string str) {
    u64 hash = 5381;
    for (u64 i = 0; i < str.length; i++)
        hash = ((hash << 5) + hash) + str.data[i];
    return hash;
}

static StringTableEntry *string_table_find_entry(StringTableEntry *entries, u32 capacity, string key) {
    u32 index = string_hash(key) & (capacity - 1);

    StringTableEntry *tombstone = null;
    for (;;) {
        StringTableEntry *entry = &entries[index];
        if (string_match(entry->key, key))
            return entry;

        if (entry->key.length == 0) {
            if (entry->empty) {
                return tombstone ? tombstone : entry;
            } else {
                if (!tombstone)
                    tombstone = entry;
            }
        }

        index = (index + 1) & (capacity - 1);
    }
}

static void string_table_resize(StringTable *table, u32 new_capacity) {
    StringTableEntry *new_entries = make_n(StringTableEntry, new_capacity);
    for (u32 i = 0; i < new_capacity; i++)
        new_entries[i].empty = true;

    table->length = 0;
    for (u32 i = 0; i < table->capacity; i++) {
        StringTableEntry *entry = &table->entries[i];
        if (entry->key.length == 0) continue;
        StringTableEntry *new_entry = string_table_find_entry(new_entries, new_capacity, entry->key);
        new_entry->key = entry->key;
        new_entry->value = entry->value;
        new_entry->empty = false;
        table->length++;
    }

    free(table->entries);
    table->entries = new_entries;
    table->capacity = new_capacity;
}

void string_table_create(StringTable *table) {
    table->length = 0;
    table->capacity = 0;
    table->entries = null;
}

void string_table_destroy(StringTable *table) {
    if (table->entries != null) {
        free(table->entries);
        table->entries = null;
    }
    table->length = 0;
    table->capacity = 0;
}

void *string_table_get(StringTable *table, string key) {
    if (table->length == 0) return null;

    StringTableEntry *entry = string_table_find_entry(table->entries, table->capacity, key);
    if (entry->key.length == 0) return null;

    return entry->value;
}

bool string_table_set(StringTable *table, string key, void *value) {
    if (table->length + 1 > table->capacity * LOAD_CAPACITY_FACTOR) {
        u32 new_capacity = ((table->capacity) < 8 ? 8 : (table->capacity) * 2);
        string_table_resize(table, new_capacity);
    }

    StringTableEntry *entry = string_table_find_entry(table->entries, table->capacity, key);
    bool is_new_entry = entry->key.length == 0;

    entry->key = key;
    entry->value = value;
    entry->empty = false;

    if (is_new_entry) table->length++;
    return is_new_entry;
}

bool string_table_remove(StringTable *table, string key) {
    if (table->length == 0) return false;

    StringTableEntry *entry = string_table_find_entry(table->entries, table->capacity, key);
    if (entry->key.length == 0) return false;

    entry->key.length = 0; // Tombstone
    entry->value = null;
    entry->empty = true;

    table->length--;
    return true;
}