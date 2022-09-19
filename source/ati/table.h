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


typedef struct {
    void *key;
    void *value;
    bool empty;
} PointerTableEntry;

typedef struct {
    u32 length;
    u32 capacity;
    PointerTableEntry *entries;
} PointerTable;

void pointer_table_create(PointerTable *table);
void pointer_table_destroy(PointerTable *table);

void *pointer_table_get(PointerTable *table, void *key);
bool pointer_table_set(PointerTable *table, void *key, void *value);