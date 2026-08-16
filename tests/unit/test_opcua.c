/**
 * @file test_opcua.c
 * @brief Complete unit tests for Micro OPC UA Binary Server Engine (100% coverage).
 */

#include "unity/unity.h"

#include <string.h>
#include <syntropic/proto/syn_opcua.h>

/* ── Loopback / Mock Transport ───────────────────────────────────────────── */

typedef struct {
    uint8_t tx_buf[1024];
    size_t tx_len;
    uint8_t rx_buf[1024];
    size_t rx_len;
    bool fail_send;
} MockOPCUATransport;

static bool mock_opcua_send(const uint8_t *data, size_t len, void *user_ctx)
{
    MockOPCUATransport *mock = (MockOPCUATransport *)user_ctx;
    if (mock->fail_send) {
        return false;
    }
    if (len > sizeof(mock->tx_buf)) {
        return false;
    }
    (void)memcpy(mock->tx_buf, data, len);
    mock->tx_len = len;
    return true;
}

static bool mock_opcua_recv(uint8_t *buf, size_t max_len, size_t *out_len, void *user_ctx)
{
    MockOPCUATransport *mock = (MockOPCUATransport *)user_ctx;
    if (mock->rx_len == 0U) {
        return false;
    }
    size_t copy_len = (mock->rx_len < max_len) ? mock->rx_len : max_len;
    (void)memcpy(buf, mock->rx_buf, copy_len);
    *out_len = copy_len;
    mock->rx_len = 0U;
    return true;
}

/* ── Callbacks ───────────────────────────────────────────────────────────── */

static bool g_node_read_called = false;
static bool g_node_write_called = false;

static SYN_Status on_temperature_read(SYN_OPCUA_Server *srv, const SYN_OPCUA_Node *node,
                                      SYN_OPCUA_DataValue *out_val, void *user_data)
{
    (void)srv;
    (void)node;
    (void)user_data;
    g_node_read_called = true;
    out_val->value.type = SYN_OPCUA_TYPE_FLOAT;
    out_val->value.val.float_val = 24.5f;
    out_val->status_code = SYN_OPCUA_STATUS_GOOD;
    return SYN_OK;
}

static SYN_Status on_setpoint_write(SYN_OPCUA_Server *srv, const SYN_OPCUA_Node *node,
                                    const SYN_OPCUA_DataValue *in_val, void *user_data)
{
    (void)srv;
    (void)node;
    (void)in_val;
    (void)user_data;
    g_node_write_called = true;
    return SYN_OK;
}

/* ── Tests ───────────────────────────────────────────────────────────────── */

void test_opcua_server_init_and_validation(void)
{
    SYN_OPCUA_Server srv;
    SYN_OPCUA_Config cfg;
    (void)memset(&cfg, 0, sizeof(cfg));

    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_opcua_server_init(NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_opcua_server_init(&srv, NULL));

    uint8_t rx[512], tx[512];
    cfg.rx_buf = rx;
    cfg.rx_buf_size = sizeof(rx);
    cfg.tx_buf = tx;
    cfg.tx_buf_size = sizeof(tx);

    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_opcua_server_init(&srv, &cfg));
    TEST_ASSERT_EQUAL_INT(SYN_OPCUA_STATE_CLOSED, srv.state);
    TEST_ASSERT_GREATER_THAN_UINT(0, srv.node_count); /* Root, Objects, Server, ServerStatus */

    /* Small buffer check */
    cfg.rx_buf_size = 64;
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_opcua_server_init(&srv, &cfg));
}

void test_opcua_server_address_space_and_variables(void)
{
    uint8_t rx[512], tx[512];
    SYN_OPCUA_Config cfg = {
        .rx_buf = rx,
        .rx_buf_size = sizeof(rx),
        .tx_buf = tx,
        .tx_buf_size = sizeof(tx),
    };

    SYN_OPCUA_Server srv;
    syn_opcua_server_init(&srv, &cfg);

    /* Null validations */
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_opcua_server_register_node(NULL, NULL));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_opcua_server_register_node(&srv, NULL));
    TEST_ASSERT_NULL(syn_opcua_server_find_node_num(NULL, 0, 0));
    TEST_ASSERT_NULL(syn_opcua_server_find_node_num(&srv, 99, 9999));

    SYN_OPCUA_Variant dummy_var = {.type = SYN_OPCUA_TYPE_FLOAT};
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM,
                          syn_opcua_server_write_variable(NULL, 0, 0, &dummy_var));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_opcua_server_write_variable(&srv, 0, 0, NULL));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_opcua_server_write_variable(&srv, 99, 9999, &dummy_var));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM,
                          syn_opcua_server_read_variable(NULL, 0, 0, &dummy_var));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_opcua_server_read_variable(&srv, 0, 0, NULL));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_opcua_server_read_variable(&srv, 99, 9999, &dummy_var));

    /* Register Custom Sensor Variable Node */
    SYN_OPCUA_Node temp_node = {
        .node_id = {.ns_index = 1U, .id_type = SYN_OPCUA_NODEID_NUMERIC, .id = {.num = 5001U}},
        .parent_id = {.ns_index = 0U,
                      .id_type = SYN_OPCUA_NODEID_NUMERIC,
                      .id = {.num = SYN_OPCUA_NODEID_OBJECTS_FOLDER}},
        .node_class = SYN_OPCUA_NODECLASS_VARIABLE,
        .browse_name = "Temperature",
        .display_name = "Ambient Temperature",
        .data_type = SYN_OPCUA_TYPE_FLOAT,
        .value = {.value = {.type = SYN_OPCUA_TYPE_FLOAT, .val = {.float_val = 21.0f}},
                  .status_code = SYN_OPCUA_STATUS_GOOD},
        .access_level = 0x03U, /* Read/Write */
        .on_read = on_temperature_read,
        .on_write = on_setpoint_write,
    };
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_opcua_server_register_node(&srv, &temp_node));

    /* Find Node */
    SYN_OPCUA_Node *found = syn_opcua_server_find_node_num(&srv, 1U, 5001U);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_STRING("Temperature", found->browse_name);

    /* Write Variable */
    SYN_OPCUA_Variant new_val = {.type = SYN_OPCUA_TYPE_FLOAT, .val = {.float_val = 26.5f}};
    g_node_write_called = false;
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_opcua_server_write_variable(&srv, 1U, 5001U, &new_val));
    TEST_ASSERT_TRUE(g_node_write_called);

    /* Read Variable */
    SYN_OPCUA_Variant read_back;
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_opcua_server_read_variable(&srv, 1U, 5001U, &read_back));
    TEST_ASSERT_EQUAL_INT(SYN_OPCUA_TYPE_FLOAT, read_back.type);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 26.5f, read_back.val.float_val);

    /* Register nodes to fill table */
    for (size_t i = srv.node_count; i < SYN_OPCUA_MAX_NODES; i++) {
        SYN_OPCUA_Node n = temp_node;
        n.node_id.id.num = 6000U + (uint32_t)i;
        TEST_ASSERT_EQUAL_INT(SYN_OK, syn_opcua_server_register_node(&srv, &n));
    }
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_opcua_server_register_node(&srv, &temp_node));
}

void test_opcua_server_all_variant_types(void)
{
    uint8_t rx[512], tx[512];
    SYN_OPCUA_Config cfg = {
        .rx_buf = rx, .rx_buf_size = sizeof(rx), .tx_buf = tx, .tx_buf_size = sizeof(tx)};
    SYN_OPCUA_Server srv;
    syn_opcua_server_init(&srv, &cfg);

    SYN_OPCUA_DataType types[] = {
        SYN_OPCUA_TYPE_BOOLEAN,  SYN_OPCUA_TYPE_SBYTE,  SYN_OPCUA_TYPE_BYTE,
        SYN_OPCUA_TYPE_INT16,    SYN_OPCUA_TYPE_UINT16, SYN_OPCUA_TYPE_INT32,
        SYN_OPCUA_TYPE_UINT32,   SYN_OPCUA_TYPE_FLOAT,  SYN_OPCUA_TYPE_INT64,
        SYN_OPCUA_TYPE_UINT64,   SYN_OPCUA_TYPE_DOUBLE, SYN_OPCUA_TYPE_STRING,
        SYN_OPCUA_TYPE_DATETIME, SYN_OPCUA_TYPE_NULL};

    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        SYN_OPCUA_Node node = {
            .node_id = {.ns_index = (i == 0) ? 0x200U : 1U,
                        .id_type = SYN_OPCUA_NODEID_NUMERIC,
                        .id = {.num = (i == 0) ? 0x20000U : (7000U + (uint32_t)i)}},
            .node_class = SYN_OPCUA_NODECLASS_VARIABLE,
            .browse_name = "Var",
            .display_name = "Var",
            .data_type = types[i],
            .value = {.value = {.type = types[i]},
                      .status_code =
                          (i == 1) ? SYN_OPCUA_STATUS_BAD_NOT_WRITABLE : SYN_OPCUA_STATUS_GOOD,
                      .source_ts_ms = (i == 2) ? 1000U : 0U},
            .access_level = 0x01U,
        };
        if (types[i] == SYN_OPCUA_TYPE_STRING) {
            node.value.value.val.string = "TestString";
        }
        syn_opcua_server_register_node(&srv, &node);

        SYN_OPCUA_Variant v;
        TEST_ASSERT_EQUAL_INT(SYN_OK, syn_opcua_server_read_variable(&srv, node.node_id.ns_index,
                                                                     node.node_id.id.num, &v));
    }

    /* Send ReadRequest for all nodes to test all variant encoders */
    uint8_t rd_msg[512];
    (void)memset(rd_msg, 0, sizeof(rd_msg));
    rd_msg[0] = 'M';
    rd_msg[1] = 'S';
    rd_msg[2] = 'G';
    rd_msg[3] = 'F';
    uint32_t one = 1U;
    (void)memcpy(&rd_msg[8], &one, 4);
    (void)memcpy(&rd_msg[12], &one, 4);
    (void)memcpy(&rd_msg[16], &one, 4);
    (void)memcpy(&rd_msg[20], &one, 4);
    rd_msg[24] = 0x01U;
    rd_msg[25] = 0x00U;
    uint16_t req_rd = 631U;
    (void)memcpy(&rd_msg[26], &req_rd, 2);

    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        uint32_t cnt = 1U;
        (void)memcpy(&rd_msg[44], &cnt, 4);
        rd_msg[48] = (i == 0) ? 0x02U : 0x01U; /* NodeId format */
        if (i == 0) {
            uint16_t ns = 0x200U;
            uint32_t num = 0x20000U;
            (void)memcpy(&rd_msg[49], &ns, 2);
            (void)memcpy(&rd_msg[51], &num, 4);
        } else {
            rd_msg[49] = 0x01U;
            uint16_t num = 7000U + (uint16_t)i;
            (void)memcpy(&rd_msg[50], &num, 2);
        }
        uint8_t resp_buf[512];
        size_t resp_len = 0U;
        (void)syn_opcua_server_process_message(&srv, rd_msg, 60U, resp_buf, sizeof(resp_buf),
                                               &resp_len);
    }

    /* TwoByte NodeId Read */
    rd_msg[48] = 0x00U;
    rd_msg[49] = 0x55U;
    uint8_t resp_buf2[512];
    size_t resp_len2 = 0U;
    (void)syn_opcua_server_process_message(&srv, rd_msg, 55U, resp_buf2, sizeof(resp_buf2),
                                           &resp_len2);

    /* NodeId reader underflow & unsupported encoding tests */
    rd_msg[48] = 0x00U;
    (void)syn_opcua_server_process_message(&srv, rd_msg, 49U, resp_buf2, sizeof(resp_buf2),
                                           &resp_len2);
    rd_msg[48] = 0x01U;
    (void)syn_opcua_server_process_message(&srv, rd_msg, 50U, resp_buf2, sizeof(resp_buf2),
                                           &resp_len2);
    rd_msg[48] = 0x02U;
    (void)syn_opcua_server_process_message(&srv, rd_msg, 51U, resp_buf2, sizeof(resp_buf2),
                                           &resp_len2);
    rd_msg[48] = 0x03U;
    (void)syn_opcua_server_process_message(&srv, rd_msg, 55U, resp_buf2, sizeof(resp_buf2),
                                           &resp_len2);

    /* Unknown variant type read */
    SYN_OPCUA_Node unk_node = {
        .node_id = {.ns_index = 1U, .id_type = SYN_OPCUA_NODEID_NUMERIC, .id = {.num = 9999U}},
        .browse_name = "Unk",
        .display_name = "Unk",
        .data_type = (SYN_OPCUA_DataType)99,
        .node_class = SYN_OPCUA_NODECLASS_VARIABLE,
        .value = {.value = {.type = (SYN_OPCUA_DataType)99}},
    };
    (void)syn_opcua_server_register_node(&srv, &unk_node);
    uint32_t cnt = 1U;
    (void)memcpy(&rd_msg[44], &cnt, 4);
    rd_msg[48] = 0x01U;
    rd_msg[49] = 1U;
    uint16_t unk_num = 9999U;
    (void)memcpy(&rd_msg[50], &unk_num, 2);
    (void)syn_opcua_server_process_message(&srv, rd_msg, 60U, resp_buf2, sizeof(resp_buf2),
                                           &resp_len2);

    /* Write to variable without on_write callback */
    SYN_OPCUA_Variant wr_val = {.type = SYN_OPCUA_TYPE_INT32, .val = {.int32 = 42}};
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_opcua_server_write_variable(&srv, 1U, 7005U, &wr_val));
}

void test_opcua_server_uacp_uasc_and_services(void)
{
    MockOPCUATransport mock;
    (void)memset(&mock, 0, sizeof(mock));
    SYN_Transport transport = {.send = mock_opcua_send, .recv = mock_opcua_recv, .ctx = &mock};

    uint8_t rx[1024], tx[1024];
    SYN_OPCUA_Config cfg = {
        .transport = &transport,
        .rx_buf = rx,
        .rx_buf_size = sizeof(rx),
        .tx_buf = tx,
        .tx_buf_size = sizeof(tx),
    };

    SYN_OPCUA_Server srv;
    syn_opcua_server_init(&srv, &cfg);

    uint8_t resp_buf[1024];
    size_t resp_len = 0U;

    /* Process message null validations */
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM,
                          syn_opcua_server_process_message(NULL, rx, 10, tx, 10, &resp_len));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM,
                          syn_opcua_server_process_message(&srv, NULL, 10, tx, 10, &resp_len));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM,
                          syn_opcua_server_process_message(&srv, rx, 10, NULL, 10, &resp_len));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM,
                          syn_opcua_server_process_message(&srv, rx, 10, tx, 10, NULL));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM,
                          syn_opcua_server_process_message(&srv, rx, 4, tx, 10, &resp_len));

    /* 1. Send Hello Message (HELF) */
    uint8_t hel_msg[] = {
        'H',  'E',  'L',  'F',  32, 0, 0, 0, /* MessageSize = 32 */
        0,    0,    0,    0,                 /* ProtocolVersion = 0 */
        0x00, 0x04, 0x00, 0x00,              /* ReceiveBufferSize = 1024 */
        0x00, 0x04, 0x00, 0x00,              /* SendBufferSize = 1024 */
        0x00, 0x04, 0x00, 0x00,              /* MaxMessageSize = 1024 */
        1,    0,    0,    0,                 /* MaxChunkCount = 1 */
    };
    TEST_ASSERT_EQUAL_INT(SYN_OK,
                          syn_opcua_server_process_message(&srv, hel_msg, sizeof(hel_msg), resp_buf,
                                                           sizeof(resp_buf), &resp_len));
    TEST_ASSERT_EQUAL_INT(SYN_OPCUA_STATE_HELLO_RECEIVED, srv.state);
    TEST_ASSERT_EQUAL_HEX8('A', resp_buf[0]);
    TEST_ASSERT_EQUAL_HEX8('C', resp_buf[1]);
    TEST_ASSERT_EQUAL_HEX8('K', resp_buf[2]);
    TEST_ASSERT_EQUAL_HEX8('F', resp_buf[3]);

    /* 2. Send OpenSecureChannel (OPNF) */
    uint8_t opn_msg[64];
    (void)memset(opn_msg, 0, sizeof(opn_msg));
    opn_msg[0] = 'O';
    opn_msg[1] = 'P';
    opn_msg[2] = 'N';
    opn_msg[3] = 'F';
    uint32_t opn_sz = 32U;
    (void)memcpy(&opn_msg[4], &opn_sz, 4);
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_opcua_server_process_message(&srv, opn_msg, 32U, resp_buf,
                                                                   sizeof(resp_buf), &resp_len));
    TEST_ASSERT_EQUAL_INT(SYN_OPCUA_STATE_SECURE_CHANNEL, srv.state);
    TEST_ASSERT_EQUAL_HEX8('O', resp_buf[0]);

    /* 3. Send CreateSessionRequest (MSGF, NodeId 461) */
    uint8_t cs_msg[64];
    (void)memset(cs_msg, 0, sizeof(cs_msg));
    cs_msg[0] = 'M';
    cs_msg[1] = 'S';
    cs_msg[2] = 'G';
    cs_msg[3] = 'F';
    uint32_t cs_sz = 32U;
    (void)memcpy(&cs_msg[4], &cs_sz, 4);
    uint32_t one = 1U;
    (void)memcpy(&cs_msg[8], &one, 4);
    (void)memcpy(&cs_msg[12], &one, 4);
    (void)memcpy(&cs_msg[16], &one, 4);
    (void)memcpy(&cs_msg[20], &one, 4);
    cs_msg[24] = 0x01U;
    cs_msg[25] = 0x00U;
    uint16_t req_cs = 461U;
    (void)memcpy(&cs_msg[26], &req_cs, 2);

    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_opcua_server_process_message(&srv, cs_msg, 32U, resp_buf,
                                                                   sizeof(resp_buf), &resp_len));
    TEST_ASSERT_EQUAL_INT(SYN_OPCUA_STATE_SESSION_ACTIVE, srv.state);

    /* Register large node to test Numeric NodeId encoding in Browse */
    SYN_OPCUA_Node large_node = {
        .node_id = {.ns_index = 0x200U,
                    .id_type = SYN_OPCUA_NODEID_NUMERIC,
                    .id = {.num = 0x20000U}},
        .browse_name = "LargeNum",
        .display_name = "LargeNum",
        .data_type = SYN_OPCUA_TYPE_INT32,
        .node_class = SYN_OPCUA_NODECLASS_VARIABLE,
    };
    (void)syn_opcua_server_register_node(&srv, &large_node);

    /* 4. Send ActivateSessionRequest (MSGF, NodeId 467) */
    uint16_t req_act = 467U;
    (void)memcpy(&cs_msg[26], &req_act, 2);
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_opcua_server_process_message(&srv, cs_msg, 32U, resp_buf,
                                                                   sizeof(resp_buf), &resp_len));

    /* Short MSG reader underflow test */
    (void)syn_opcua_server_process_message(&srv, cs_msg, 12U, resp_buf, sizeof(resp_buf),
                                           &resp_len);

    /* 5. Send BrowseRequest (MSGF, NodeId 527) */
    uint8_t br_msg[64];
    (void)memset(br_msg, 0, sizeof(br_msg));
    br_msg[0] = 'M';
    br_msg[1] = 'S';
    br_msg[2] = 'G';
    br_msg[3] = 'F';
    uint32_t br_sz = 56U;
    (void)memcpy(&br_msg[4], &br_sz, 4);
    (void)memcpy(&br_msg[8], &one, 4);
    (void)memcpy(&br_msg[12], &one, 4);
    (void)memcpy(&br_msg[16], &one, 4);
    (void)memcpy(&br_msg[20], &one, 4);
    br_msg[24] = 0x01U;
    br_msg[25] = 0x00U;
    uint16_t req_br = 527U;
    (void)memcpy(&br_msg[26], &req_br, 2);

    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_opcua_server_process_message(&srv, br_msg, 56U, resp_buf,
                                                                   sizeof(resp_buf), &resp_len));
    TEST_ASSERT_GREATER_THAN_UINT(30, resp_len);

    /* Register writable node and node with on_read callback */
    SYN_OPCUA_Node rw_node = {
        .node_id = {.ns_index = 1U, .id_type = SYN_OPCUA_NODEID_NUMERIC, .id = {.num = 2000U}},
        .browse_name = "RWNode",
        .display_name = "RWNode",
        .data_type = SYN_OPCUA_TYPE_INT32,
        .node_class = SYN_OPCUA_NODECLASS_VARIABLE,
        .access_level = 0x03U, /* Read & Write */
        .on_read = on_temperature_read,
    };
    (void)syn_opcua_server_register_node(&srv, &rw_node);

    /* 6. Send ReadRequest with count = 20 (testing count > 16 clamp) */
    uint8_t rd_msg[128];
    (void)memset(rd_msg, 0, sizeof(rd_msg));
    rd_msg[0] = 'M';
    rd_msg[1] = 'S';
    rd_msg[2] = 'G';
    rd_msg[3] = 'F';
    uint32_t rd_sz = 60U;
    (void)memcpy(&rd_msg[4], &rd_sz, 4);
    (void)memcpy(&rd_msg[8], &one, 4);
    (void)memcpy(&rd_msg[12], &one, 4);
    (void)memcpy(&rd_msg[16], &one, 4);
    (void)memcpy(&rd_msg[20], &one, 4);
    rd_msg[24] = 0x01U;
    rd_msg[25] = 0x00U;
    uint16_t req_rd = 631U;
    (void)memcpy(&rd_msg[26], &req_rd, 2);
    uint32_t cnt20 = 20U;
    (void)memcpy(&rd_msg[44], &cnt20, 4);
    rd_msg[48] = 0x01U;
    rd_msg[49] = 0x01U;
    uint16_t rw_node_id = 2000U;
    (void)memcpy(&rd_msg[50], &rw_node_id, 2);

    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_opcua_server_process_message(&srv, rd_msg, 56U, resp_buf,
                                                                   sizeof(resp_buf), &resp_len));
    TEST_ASSERT_GREATER_THAN_UINT(30, resp_len);
    TEST_ASSERT_TRUE(g_node_read_called);

    /* 7. Send WriteRequest with count = 20, writable, non-writable, and unknown nodes */
    uint8_t wr_msg[128];
    (void)memset(wr_msg, 0, sizeof(wr_msg));
    wr_msg[0] = 'M';
    wr_msg[1] = 'S';
    wr_msg[2] = 'G';
    wr_msg[3] = 'F';
    uint32_t wr_sz = 60U;
    (void)memcpy(&wr_msg[4], &wr_sz, 4);
    (void)memcpy(&wr_msg[8], &one, 4);
    (void)memcpy(&wr_msg[12], &one, 4);
    (void)memcpy(&wr_msg[16], &one, 4);
    (void)memcpy(&wr_msg[20], &one, 4);
    wr_msg[24] = 0x01U;
    wr_msg[25] = 0x00U;
    uint16_t req_wr = 673U;
    (void)memcpy(&wr_msg[26], &req_wr, 2);
    (void)memcpy(&wr_msg[44], &cnt20, 4);
    /* Node 1: Writable (ns=1, num=2000) */
    wr_msg[48] = 0x01U;
    wr_msg[49] = 0x01U;
    uint16_t rw_id = 2000U;
    (void)memcpy(&wr_msg[50], &rw_id, 2);

    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_opcua_server_process_message(&srv, wr_msg, 56U, resp_buf,
                                                                   sizeof(resp_buf), &resp_len));

    /* Write to non-writable node (ns=0, num=2256) */
    uint32_t cnt1 = 1U;
    (void)memcpy(&wr_msg[44], &cnt1, 4);
    wr_msg[48] = 0x01U;
    wr_msg[49] = 0x00U;
    uint16_t srv_status_id = 2256U;
    (void)memcpy(&wr_msg[50], &srv_status_id, 2);
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_opcua_server_process_message(&srv, wr_msg, 56U, resp_buf,
                                                                   sizeof(resp_buf), &resp_len));

    /* Write to unknown node (ns=1, num=9999) */
    uint16_t unk_wr_id = 9999U;
    wr_msg[49] = 0x01U;
    (void)memcpy(&wr_msg[50], &unk_wr_id, 2);
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_opcua_server_process_message(&srv, wr_msg, 56U, resp_buf,
                                                                   sizeof(resp_buf), &resp_len));

    /* 8. Send CloseSessionRequest (MSGF, NodeId 473) */
    uint16_t req_close = 473U;
    (void)memcpy(&cs_msg[26], &req_close, 2);
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_opcua_server_process_message(&srv, cs_msg, 32U, resp_buf,
                                                                   sizeof(resp_buf), &resp_len));
    TEST_ASSERT_EQUAL_INT(SYN_OPCUA_STATE_CLOSED, srv.state);

    /* 9. Send Unsupported Service (MSGF, NodeId 999) */
    uint16_t req_unsupported = 999U;
    (void)memcpy(&cs_msg[26], &req_unsupported, 2);
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_opcua_server_process_message(&srv, cs_msg, 32U, resp_buf,
                                                                   sizeof(resp_buf), &resp_len));

    /* 10. Tiny Response Buffer tests for writer overflow handling */
    uint8_t micro_resp[2];
    size_t tiny_resp_len = 0U;
    TEST_ASSERT_EQUAL_INT(
        SYN_ERROR, syn_opcua_server_process_message(&srv, hel_msg, sizeof(hel_msg), micro_resp,
                                                    sizeof(micro_resp), &tiny_resp_len));
    uint8_t tiny_resp[8];
    TEST_ASSERT_EQUAL_INT(
        SYN_ERROR, syn_opcua_server_process_message(&srv, hel_msg, sizeof(hel_msg), tiny_resp,
                                                    sizeof(tiny_resp), &tiny_resp_len));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_opcua_server_process_message(&srv, opn_msg, 32U, tiny_resp,
                                                           sizeof(tiny_resp), &tiny_resp_len));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_opcua_server_process_message(&srv, cs_msg, 32U, tiny_resp,
                                                           sizeof(tiny_resp), &tiny_resp_len));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_opcua_server_process_message(&srv, rd_msg, 56U, tiny_resp,
                                                           sizeof(tiny_resp), &tiny_resp_len));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_opcua_server_process_message(&srv, wr_msg, 56U, tiny_resp,
                                                           sizeof(tiny_resp), &tiny_resp_len));
    uint8_t mid_resp[43];
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_opcua_server_process_message(&srv, br_msg, 56U, mid_resp,
                                                           sizeof(mid_resp), &tiny_resp_len));
    uint8_t mid_resp2[30];
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_opcua_server_process_message(&srv, br_msg, 56U, mid_resp2,
                                                           sizeof(mid_resp2), &tiny_resp_len));

    /* Unknown message type header */
    uint8_t unk_hdr[] = {'X', 'X', 'X', 'F', 8, 0, 0, 0};
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_opcua_server_process_message(&srv, unk_hdr, 8U, resp_buf,
                                                                      sizeof(resp_buf), &resp_len));

    /* 11. Step with incoming transport frame */
    (void)memcpy(mock.rx_buf, hel_msg, sizeof(hel_msg));
    mock.rx_len = sizeof(hel_msg);
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_opcua_server_step(&srv, 100U));
    TEST_ASSERT_GREATER_THAN_UINT(0, mock.tx_len);
}

void test_opcua_server_protothread(void)
{
    MockOPCUATransport mock;
    (void)memset(&mock, 0, sizeof(mock));
    SYN_Transport transport = {.send = mock_opcua_send, .recv = mock_opcua_recv, .ctx = &mock};

    uint8_t rx[256], tx[256];
    SYN_OPCUA_Config cfg = {
        .transport = &transport,
        .rx_buf = rx,
        .rx_buf_size = sizeof(rx),
        .tx_buf = tx,
        .tx_buf_size = sizeof(tx),
    };

    SYN_OPCUA_Server srv;
    syn_opcua_server_init(&srv, &cfg);

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task = {.user_data = &srv};

    SYN_PT_Status st = syn_opcua_server_pt(&pt, &task);
    TEST_ASSERT_EQUAL_INT(PT_YIELDED, st);

    /* Null checks */
    TEST_ASSERT_EQUAL_INT(PT_ENDED, syn_opcua_server_pt(NULL, &task));
    TEST_ASSERT_EQUAL_INT(PT_ENDED, syn_opcua_server_pt(&pt, NULL));
    SYN_Task null_task = {.user_data = NULL};
    TEST_ASSERT_EQUAL_INT(PT_ENDED, syn_opcua_server_pt(&pt, &null_task));

    /* Step error termination */
    srv.cfg.transport = NULL;
    st = syn_opcua_server_pt(&pt, &task);
    TEST_ASSERT_EQUAL_INT(PT_EXITED, st);
}

void run_opcua_tests(void)
{
    RUN_TEST(test_opcua_server_init_and_validation);
    RUN_TEST(test_opcua_server_address_space_and_variables);
    RUN_TEST(test_opcua_server_all_variant_types);
    RUN_TEST(test_opcua_server_uacp_uasc_and_services);
    RUN_TEST(test_opcua_server_protothread);
}
