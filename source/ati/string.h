#pragma once

#include "basic.h"

typedef char *cstring;

typedef struct string {
    i8 *data;
    u64 length;
} string;

#define strp(s) (i32)s.length, s.data

#define str(s) (string) { .data = (i8 *) (s), .length = sizeof(s) - 1 }
#define rawstr(d, l) (string) { .data = (i8 *) (d), .length = (l) }

string make_string(u64 length);

string string_from_cstring(cstring s);
cstring string_to_cstring(string s);

bool string_match(string s1, string s2);
bool string_match_cstring(string str, cstring cstr);