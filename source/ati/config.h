#pragma once
#include "basic.h"
#include "string.h"

typedef struct entry {
    string value;
    string tag;
} entry;

typedef struct option {
    string key;
    entry *entries;
} option;

option *options_parse(string filename, Buffer buffer);
bool options_get(option *options, string key, string *value);
bool options_get_default(option *options, string key, string *value, string default_value);
bool options_get_list(option *options, string key, entry **value);
