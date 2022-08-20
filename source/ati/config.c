#include "config.h"

bool options_parse(string filename, Vec(option) *options) {
    cstring filename_as_cstring = string_to_cstring(filename);
    FILE *file = fopen(filename_as_cstring, "r");
    free(filename_as_cstring);

    if (!file)
        return false;

    cstring line = NULL;
    size_t line_length = 0;
    while (line = fgetln(file, )) {

    }
}

bool options_get(option *options, string key, string *value) {

}

bool options_get_default(option *options, string key, string *value, string default_value) {

}

bool options_get_list(option *options, string key, Vec(string) *value) {

}