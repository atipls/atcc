#include "ati/basic.h"
#include "ati/config.h"
#include "ati/utest.h"
#include "ati/utils.h"

static int ati_test_variant_equality(void) {
    struct VariantTest {
        Variant variant;
        VariantKind expected_kind;
    } tests[] = {
            {variant_none(), VARIANT_NONE},
            {variant_i8(0), VARIANT_I8},
            {variant_u8(0), VARIANT_U8},
            {variant_i16(0), VARIANT_I16},
            {variant_u16(0), VARIANT_U16},
            {variant_i32(0), VARIANT_I32},
            {variant_u32(0), VARIANT_U32},
            {variant_i64(0), VARIANT_I64},
            {variant_u64(0), VARIANT_U64},
            {variant_f32(0), VARIANT_F32},
            {variant_f64(0), VARIANT_F64},
            {variant_string(NULL, 0), VARIANT_STRING},
            {variant_array(NULL, 0), VARIANT_ARRAY},
    };

    for (int i = 0; i < array_length(tests); i++) {
        struct VariantTest *test = &tests[i];
        UASSERT(test->variant.kind == test->expected_kind);
    }

    // Only equal to itself (same type, same value)
    for (int i = 0; i < array_length(tests); i++) {
        struct VariantTest *test = &tests[i];
        for (int j = 0; j < array_length(tests); j++) {
            struct VariantTest *other = &tests[j];
            if (i == j) {
                UASSERT(variant_equals(&test->variant, &other->variant));
            } else {
                UASSERT(!variant_equals(&test->variant, &other->variant));
            }
        }
    }

    return UTEST_PASS;
}

static int ati_test_config_parses_and_loads_values(void) {
    const char config[] =
            "# comment\n"
            "key1 = \"value1\"#comment\n"
            "key2 = \"value2\"\n"
            "array1 = [\"value3\", \"value4\"]\n"
            "array2 = [\"value5\", @tag1 \"value6\"]\n"
            "array3 = [@tag2\"value7\",#comment \n \"value8\"]\n";

    option *options = options_parse(str("test.cnf"), (Buffer){.data = (i8 *) config, .length = sizeof(config) - 1});
    UASSERT(options != null);

    string option_value;
    entry *option_list = null;

    UASSERT(options_get(options, str("key1"), &option_value));
    UASSERT(string_match(option_value, str("value1")));

    UASSERT(options_get(options, str("key2"), &option_value));
    UASSERT(string_match(option_value, str("value2")));

    UASSERT(!options_get(options, str("key3"), &option_value));
    UASSERT(!options_get(options, str("key4"), &option_value));

    UASSERT(!options_get_default(options, str("key3"), &option_value, str("default1")));
    UASSERT(string_match(option_value, str("default1")));

    UASSERT(!options_get_default(options, str("key4"), &option_value, str("default2")));
    UASSERT(string_match(option_value, str("default2")));

    UASSERT(options_get_list(options, str("array1"), &option_list));
    UASSERT(option_list != null);

    UASSERT(string_match(option_list[0].value, str("value3")));
    UASSERT(option_list[0].tag.length == 0);
    UASSERT(option_list[0].tag.data == null);

    UASSERT(string_match(option_list[1].value, str("value4")));
    UASSERT(option_list[1].tag.length == 0);
    UASSERT(option_list[1].tag.data == null);

    UASSERT(options_get_list(options, str("array2"), &option_list));
    UASSERT(option_list != null);

    UASSERT(string_match(option_list[0].value, str("value5")));
    UASSERT(option_list[0].tag.length == 0);
    UASSERT(option_list[0].tag.data == null);

    UASSERT(string_match(option_list[1].value, str("value6")));
    UASSERT(string_match(option_list[1].tag, str("tag1")));

    UASSERT(options_get_list(options, str("array3"), &option_list));
    UASSERT(option_list != null);

    UASSERT(string_match(option_list[0].value, str("value7")));
    UASSERT(string_match(option_list[0].tag, str("tag2")));

    UASSERT(string_match(option_list[1].value, str("value8")));
    UASSERT(option_list[1].tag.length == 0);
    UASSERT(option_list[1].tag.data == null);

    UASSERT(!options_get_list(options, str("array4"), &option_list));

    return UTEST_PASS;
}

void ati_register_utest(void) {
	UTest tests[] = {
		{ str("variant equality"), ati_test_variant_equality },
		{ str("config parses and loads values"), ati_test_config_parses_and_loads_values },
	};

	utest_register(str("ati"), tests, array_length(tests));
}
