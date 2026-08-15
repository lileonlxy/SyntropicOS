/**
 * @file fuzz_json.c
 * @brief libFuzzer target for syn_json reader.
 */

#include "syntropic/util/syn_json_read.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size == 0 || size > 4096)
        return 0;

    /* Create null-terminated mutable copy of data */
    char json_buf[4097];
    memcpy(json_buf, data, size);
    json_buf[size] = '\0';

    SYN_JsonReader reader;
    if (syn_json_parse(&reader, json_buf, size)) {
        int32_t i_val = 0;
        bool b_val = false;
        char str_buf[64];

        /* Attempt to query various JSON keys */
        syn_json_get_int(&reader, "val", &i_val);
        syn_json_get_bool(&reader, "enable", &b_val);
        syn_json_get_str(&reader, "name", str_buf, sizeof(str_buf));
        syn_json_is_null(&reader, "null_key");
        syn_json_get_type(&reader, "type_key");
    }

    return 0;
}
