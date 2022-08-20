#pragma once
#include "basic.h"
#include "string.h"

typedef struct option {
    string key;
    string *values;
} option;

bool options_parse(string filename, option *options);
bool options_get(option *options, string key, string *value);
bool options_get_default(option *options, string key, string *value, string default_value);
bool options_get_list(option *options, string key, string *value);