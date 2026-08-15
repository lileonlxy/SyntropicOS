/**
 * @file test_json_write.c
 * @brief Tests for the JSON writer.
 */

#include "syntropic/util/syn_json_write.h"
#include "unity/unity.h"

#include <string.h>

/* ── Helpers ────────────────────────────────────────────────────────────── */

static char buf[512];
static SYN_JsonWriter w;

static void reset(void)
{
    memset(buf, 0, sizeof(buf));
    syn_json_init(&w, buf, sizeof(buf));
}

/* ── Tests ──────────────────────────────────────────────────────────────── */

void test_json_empty_object(void)
{
    reset();
    syn_json_obj_open(&w);
    syn_json_obj_close(&w);
    TEST_ASSERT_EQUAL_STRING("{}", syn_json_str(&w));
    TEST_ASSERT_TRUE(syn_json_ok(&w));
}

void test_json_empty_array(void)
{
    reset();
    syn_json_arr_open(&w);
    syn_json_arr_close(&w);
    TEST_ASSERT_EQUAL_STRING("[]", syn_json_str(&w));
}

void test_json_key_str(void)
{
    reset();
    syn_json_obj_open(&w);
    syn_json_key_str(&w, "name", "SyntropicOS");
    syn_json_obj_close(&w);
    TEST_ASSERT_EQUAL_STRING("{\"name\":\"SyntropicOS\"}", syn_json_str(&w));
}

void test_json_key_int(void)
{
    reset();
    syn_json_obj_open(&w);
    syn_json_key_int(&w, "value", -42);
    syn_json_obj_close(&w);
    TEST_ASSERT_EQUAL_STRING("{\"value\":-42}", syn_json_str(&w));
}

void test_json_key_uint(void)
{
    reset();
    syn_json_obj_open(&w);
    syn_json_key_uint(&w, "count", 4294967295u);
    syn_json_obj_close(&w);
    TEST_ASSERT_EQUAL_STRING("{\"count\":4294967295}", syn_json_str(&w));
}

void test_json_key_bool(void)
{
    reset();
    syn_json_obj_open(&w);
    syn_json_key_bool(&w, "connected", true);
    syn_json_key_bool(&w, "error", false);
    syn_json_obj_close(&w);
    TEST_ASSERT_EQUAL_STRING("{\"connected\":true,\"error\":false}", syn_json_str(&w));
}

void test_json_key_null(void)
{
    reset();
    syn_json_obj_open(&w);
    syn_json_key_null(&w, "data");
    syn_json_obj_close(&w);
    TEST_ASSERT_EQUAL_STRING("{\"data\":null}", syn_json_str(&w));
}

void test_json_multiple_keys(void)
{
    reset();
    syn_json_obj_open(&w);
    syn_json_key_str(&w, "device", "esp32");
    syn_json_key_int(&w, "uptime", 12345);
    syn_json_key_bool(&w, "wifi", true);
    syn_json_obj_close(&w);
    TEST_ASSERT_EQUAL_STRING("{\"device\":\"esp32\",\"uptime\":12345,\"wifi\":true}",
                             syn_json_str(&w));
}

void test_json_nested_object(void)
{
    reset();
    syn_json_obj_open(&w);
    syn_json_key(&w, "net");
    syn_json_obj_open(&w);
    syn_json_key_str(&w, "ip", "192.168.1.1");
    syn_json_key_int(&w, "port", 80);
    syn_json_obj_close(&w);
    syn_json_obj_close(&w);
    TEST_ASSERT_EQUAL_STRING("{\"net\":{\"ip\":\"192.168.1.1\",\"port\":80}}", syn_json_str(&w));
}

void test_json_array_values(void)
{
    reset();
    syn_json_obj_open(&w);
    syn_json_key(&w, "sensors");
    syn_json_arr_open(&w);
    syn_json_val_int(&w, 23);
    syn_json_val_int(&w, 45);
    syn_json_val_int(&w, 67);
    syn_json_arr_close(&w);
    syn_json_obj_close(&w);
    TEST_ASSERT_EQUAL_STRING("{\"sensors\":[23,45,67]}", syn_json_str(&w));
}

void test_json_string_array(void)
{
    reset();
    syn_json_arr_open(&w);
    syn_json_val_str(&w, "alpha");
    syn_json_val_str(&w, "beta");
    syn_json_arr_close(&w);
    TEST_ASSERT_EQUAL_STRING("[\"alpha\",\"beta\"]", syn_json_str(&w));
}

void test_json_escaping(void)
{
    reset();
    syn_json_obj_open(&w);
    syn_json_key_str(&w, "msg", "hello \"world\"\nline2");
    syn_json_obj_close(&w);
    TEST_ASSERT_EQUAL_STRING("{\"msg\":\"hello \\\"world\\\"\\nline2\"}", syn_json_str(&w));
}

void test_json_overflow(void)
{
    char small[16];
    SYN_JsonWriter sw;
    syn_json_init(&sw, small, sizeof(small));
    syn_json_obj_open(&sw);
    syn_json_key_str(&sw, "this_is_way_too_long", "value");
    syn_json_obj_close(&sw);
    TEST_ASSERT_FALSE(syn_json_ok(&sw));
}

void test_json_zero(void)
{
    reset();
    syn_json_obj_open(&w);
    syn_json_key_int(&w, "zero", 0);
    syn_json_key_uint(&w, "uzero", 0);
    syn_json_obj_close(&w);
    TEST_ASSERT_EQUAL_STRING("{\"zero\":0,\"uzero\":0}", syn_json_str(&w));
}

void test_json_int_min(void)
{
    reset();
    syn_json_obj_open(&w);
    syn_json_key_int(&w, "min", -2147483647 - 1);
    syn_json_obj_close(&w);
    TEST_ASSERT_EQUAL_STRING("{\"min\":-2147483648}", syn_json_str(&w));
}

void test_json_bool_array(void)
{
    reset();
    syn_json_arr_open(&w);
    syn_json_val_bool(&w, true);
    syn_json_val_bool(&w, false);
    syn_json_arr_close(&w);
    TEST_ASSERT_EQUAL_STRING("[true,false]", syn_json_str(&w));
}

/** Extra escape chars: \\ \r \t — exercises lines 65, 67, 68 */
static void test_json_write_control_character_escaping(void)
{
    reset();
    syn_json_obj_open(&w);
    syn_json_key_str(&w, "s", "a\\b\rc\td"); /* backslash, CR, TAB */
    syn_json_obj_close(&w);
    const char *out = syn_json_str(&w);
    TEST_ASSERT_NOT_NULL(strstr(out, "\\\\")); /* escaped backslash */
    TEST_ASSERT_NOT_NULL(strstr(out, "\\r"));  /* escaped CR */
    TEST_ASSERT_NOT_NULL(strstr(out, "\\t"));  /* escaped TAB */

    /* Test null string values and ASCII control char skipping (< 0x20) */
    reset();
    syn_json_obj_open(&w);
    syn_json_key_str(&w, "null_val", NULL);
    syn_json_key(&w, "arr");
    syn_json_arr_open(&w);
    syn_json_val_str(&w, NULL);
    syn_json_val_str(&w, "ctrl\x01test");
    syn_json_arr_close(&w);
    syn_json_obj_close(&w);
    TEST_ASSERT_EQUAL_STRING("{\"null_val\":\"\",\"arr\":[\"\",\"ctrltest\"]}", syn_json_str(&w));
}

/** jw_puts overflow mid-string — exercises lines 45-46 */
static void test_json_write_puts_overflow(void)
{
    char buf2[12]; /* very small */
    SYN_JsonWriter w2;
    syn_json_init(&w2, buf2, sizeof(buf2));
    syn_json_obj_open(&w2);
    /* Key + escape sequence will exceed small buffer */
    syn_json_key_str(&w2, "k", "\\escaped_and_long");
    TEST_ASSERT_FALSE(syn_json_ok(&w2));
}

/** syn_json_val_uint — exercises lines 290-295 */
static void test_json_val_uint(void)
{
    reset();
    syn_json_obj_open(&w);
    syn_json_key(&w, "n");
    syn_json_val_uint(&w, 42u);
    syn_json_obj_close(&w);
    TEST_ASSERT_NOT_NULL(strstr(syn_json_str(&w), "42"));
}

/* ── Test group ────────────────────────────────────────────────────────── */

static void test_json_write_max_depth_overflow_and_null_checks(void)
{
    reset();
    /* Open object up to SYN_JSON_MAX_DEPTH */
    for (int i = 0; i < 16; i++) {
        syn_json_obj_open(&w);
    }
    /* 17th open triggers max depth overflow */
    syn_json_obj_open(&w);
    TEST_ASSERT_FALSE(syn_json_ok(&w));

    reset();
    for (int i = 0; i < 16; i++) {
        syn_json_arr_open(&w);
    }
    syn_json_arr_open(&w);
    TEST_ASSERT_FALSE(syn_json_ok(&w));
}

static void test_json_null_writer_and_comma_checks(void)
{
    /* Array containing objects to test needs_comma on obj_open */
    reset();
    syn_json_arr_open(&w);
    syn_json_obj_open(&w);
    syn_json_obj_close(&w);
    syn_json_obj_open(&w);
    syn_json_obj_close(&w);
    syn_json_arr_close(&w);
    TEST_ASSERT_EQUAL_STRING("[{},{}]", syn_json_str(&w));

    /* Array containing nested arrays to test needs_comma on arr_open */
    reset();
    syn_json_arr_open(&w);
    syn_json_arr_open(&w);
    syn_json_arr_close(&w);
    syn_json_arr_open(&w);
    syn_json_arr_close(&w);
    syn_json_arr_close(&w);
    TEST_ASSERT_EQUAL_STRING("[[],[]]", syn_json_str(&w));
}

static void test_json_write_jw_puts_overflow_branch(void)
{
    char buf[10];
    SYN_JsonWriter w;
    syn_json_init(&w, buf, sizeof(buf));
    syn_json_obj_open(&w);
    syn_json_key_str(&w, "k", "abcdefghijk");
    TEST_ASSERT_TRUE(w.overflow);
}

static void test_json_write_max_depth_overflow(void)
{
    char buf[256];
    SYN_JsonWriter w;
    syn_json_init(&w, buf, sizeof(buf));
    for (int i = 0; i < 16; i++) {
        syn_json_obj_open(&w);
    }
    TEST_ASSERT_TRUE(w.overflow);
}

static void test_json_close_zero_depth(void)
{
    reset();
    /* Closing at depth 0 should not underflow depth */
    syn_json_obj_close(&w);
    TEST_ASSERT_EQUAL_UINT8(0, w.depth);
    TEST_ASSERT_EQUAL_STRING("}", syn_json_str(&w));

    reset();
    syn_json_arr_close(&w);
    TEST_ASSERT_EQUAL_UINT8(0, w.depth);
    TEST_ASSERT_EQUAL_STRING("]", syn_json_str(&w));
}

void run_json_write_tests(void)
{
    RUN_TEST(test_json_empty_object);
    RUN_TEST(test_json_empty_array);
    RUN_TEST(test_json_key_str);
    RUN_TEST(test_json_key_int);
    RUN_TEST(test_json_key_uint);
    RUN_TEST(test_json_key_bool);
    RUN_TEST(test_json_key_null);
    RUN_TEST(test_json_multiple_keys);
    RUN_TEST(test_json_nested_object);
    RUN_TEST(test_json_array_values);
    RUN_TEST(test_json_string_array);
    RUN_TEST(test_json_escaping);
    RUN_TEST(test_json_overflow);
    RUN_TEST(test_json_zero);
    RUN_TEST(test_json_int_min);
    RUN_TEST(test_json_bool_array);
    RUN_TEST(test_json_write_control_character_escaping);
    RUN_TEST(test_json_write_puts_overflow);
    RUN_TEST(test_json_val_uint);
    RUN_TEST(test_json_write_max_depth_overflow_and_null_checks);
    RUN_TEST(test_json_null_writer_and_comma_checks);
    RUN_TEST(test_json_write_jw_puts_overflow_branch);
    RUN_TEST(test_json_write_max_depth_overflow);
    RUN_TEST(test_json_close_zero_depth);
}
