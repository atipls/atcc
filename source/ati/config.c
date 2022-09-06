#include "config.h"
#include "utils.h"

bool options_parse(string filename, option *options) {
    Buffer buffer = read_file(filename);
    if (!buffer.length)
        return false;



    return true;
}

bool options_get(option *options, string key, string *value) {
    return false;
}

bool options_get_default(option *options, string key, string *value, string default_value) {
    return false;
}

bool options_get_list(option *options, string key, string *value) {
    return false;
}