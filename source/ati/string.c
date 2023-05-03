#include "string.h"
#include <stdarg.h>
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

bool string_to_f64(string s, f64 *value) {
    cstring api = string_to_cstring(s);
    cstring end = null;
    *value = strtod(api, &end);
    bool success = *end == 0;
    free(api);
    return success;
}

bool string_to_u64(string s, u64 *value) {
    cstring api = string_to_cstring(s);
    cstring end = null;
    *value = strtoull(api, &end, 10);
    bool success = *end == 0;
    free(api);
    return success;
}

string *string_split(string s, string delimiter, u64 *count) {
    string *result = null;
    u64 result_count = 0;
    u64 start = 0;
    for (u64 i = 0; i < s.length; i++) {
        if (i + delimiter.length > s.length)
            break;
        if (string_match(rawstr(s.data + i, delimiter.length), delimiter)) {
            result = realloc(result, sizeof(string) * (result_count + 1));
            result[result_count] = rawstr(s.data + start, i - start);
            result_count++;
            i += delimiter.length - 1;
            start = i + 1;
        }
    }
    if (start < s.length) {
        result = realloc(result, sizeof(string) * (result_count + 1));
        result[result_count] = rawstr(s.data + start, s.length - start);
        result_count++;
    }
    *count = result_count;
    return result;
}

string string_append(string s1, string s2) {
    string result = make_string(s1.length + s2.length);
    memcpy(result.data, s1.data, s1.length);
    memcpy(result.data + s1.length, s2.data, s2.length);
    return result;
}

string string_append_cstring(string s, cstring cstr) {
    string cstr_str = string_from_cstring(cstr);
    string result = string_append(s, cstr_str);
    free(cstr_str.data);
    return result;
}

static string string_format_va(string format, va_list args) {
    va_list args_copy;
    va_copy(args_copy, args);
    i32 length = vsnprintf(null, 0, (char *) format.data, args_copy);
    va_end(args_copy);
    string result = make_string(length);
    vsnprintf((char *) result.data, length + 1, (char *) format.data, args);
    return result;
}

string string_format(string format, ...) {
    va_list args;
    va_start(args, format);
    string result = string_format_va(format, args);
    va_end(args);
    return result;
}