#pragma once

#include "basic.h"
#include "string.h"

typedef struct {
    string key;
    void *value;
    bool empty;
} StringTableEntry;

typedef struct {
    u32 length;
    u32 capacity;
    StringTableEntry *entries;
} StringTable;

void string_table_create(StringTable *table);
void string_table_destroy(StringTable *table);

void *string_table_get(StringTable *table, string key);
bool string_table_set(StringTable *table, string key, void *value);
bool string_table_remove(StringTable *table, string key);