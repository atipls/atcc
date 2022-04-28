#include "string.h"
#include <string.h>

string make_string(u64 length) {
    string s = {0};
    s.data = (i8 *) malloc(length);
    s.length = length;
    return s;
}

string string_from_cstring(cstring s) {
    return rawstr(s, strlen((char *) s));
}

cstring string_to_cstring(string s) {
    cstring result = malloc(s.length + 1);
    memcpy(result, s.data, s.length);
    result[s.length] = '\0';
    return result;
}

bool string_match(string s1, string s2) {
    if (s1.length != s2.length)
        return false;
    return !memcmp(s1.data, s2.data, s1.length);
}

bool string_match_cstring(string s1, cstring s2) {
    return string_match(s1, string_from_cstring(s2));
}