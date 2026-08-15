#include "syntropic/net/syn_eth.h"
#include "syntropic/net/syn_tcp.h"
#include "unity/unity.h"

#include <string.h>

static SYN_ETH eth;
static SYN_TCP tcp;
static SYN_Task task;

void test_tcp_init_and_listen(void)
{
    uint8_t mac[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint32_t ip = 0xA9FE0164;

    TEST_ASSERT_EQUAL(SYN_OK, syn_eth_init(&eth, mac, ip));
    TEST_ASSERT_EQUAL(SYN_OK, syn_tcp_init(&tcp, &eth));

    TEST_ASSERT_EQUAL(SYN_OK, syn_tcp_listen(&tcp, 80));
    TEST_ASSERT_EQUAL(SYN_TCP_LISTEN, tcp.conns[0].state);
    TEST_ASSERT_EQUAL(80, tcp.conns[0].local_port);
}

void test_tcp_checksum_calc(void)
{
    uint32_t src_ip = 0xC0A80101; /* 192.168.1.1 */
    uint32_t dst_ip = 0xC0A80102; /* 192.168.1.2 */
    uint8_t dummy_seg[20] = {0};

    uint16_t csum = syn_tcp_checksum(src_ip, dst_ip, dummy_seg, sizeof(dummy_seg));
    TEST_ASSERT_NOT_EQUAL(0, csum);
}

void test_tcp_syn_ack_handshake(void)
{
    uint8_t mac[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint32_t ip = 0xA9FE0164;

    syn_eth_init(&eth, mac, ip);
    syn_tcp_init(&tcp, &eth);
    syn_tcp_listen(&tcp, 80);

    /* Construct raw SYN Ethernet frame */
    uint8_t syn_frame[54];
    memset(syn_frame, 0, sizeof(syn_frame));
    uint8_t client_mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x66};
    memcpy(&syn_frame[0], mac, 6);        /* Dst MAC */
    memcpy(&syn_frame[6], client_mac, 6); /* Src MAC */
    syn_frame[12] = 0x08;
    syn_frame[13] = 0x00; /* IPv4 */

    syn_frame[14] = 0x45;
    syn_frame[16] = 0x00;
    syn_frame[17] = 40; /* Length 40 */
    syn_frame[23] = 6;  /* Protocol TCP */
    syn_frame[26] = 169;
    syn_frame[27] = 254;
    syn_frame[28] = 1;
    syn_frame[29] = 1; /* Src IP */
    syn_frame[30] = 169;
    syn_frame[31] = 254;
    syn_frame[32] = 1;
    syn_frame[33] = 100; /* Dst IP */

    syn_frame[34] = 0xC0;
    syn_frame[35] = 0x00; /* Src Port 49152 */
    syn_frame[36] = 0x00;
    syn_frame[37] = 80; /* Dst Port 80 */
    syn_frame[38] = 0x00;
    syn_frame[39] = 0x00;
    syn_frame[40] = 0x00;
    syn_frame[41] = 0x01;             /* Seq 1 */
    syn_frame[46] = 0x50;             /* Data Offset 5 (20 bytes) */
    syn_frame[47] = SYN_TCP_FLAG_SYN; /* SYN Flag */

    uint8_t tx_out[128];
    size_t tx_len = 0;

    SYN_Status st = syn_tcp_process_packet(&tcp, syn_frame, sizeof(syn_frame), tx_out, &tx_len);
    TEST_ASSERT_EQUAL(SYN_OK, st);
    TEST_ASSERT_EQUAL(SYN_TCP_SYN_RCVD, tcp.conns[0].state);
    TEST_ASSERT_GREATER_THAN(0, tx_len);
    TEST_ASSERT_EQUAL_UINT8(SYN_TCP_FLAG_SYN | SYN_TCP_FLAG_ACK, tx_out[47]);

    /* Now send ACK packet */
    syn_frame[47] = SYN_TCP_FLAG_ACK;
    st = syn_tcp_process_packet(&tcp, syn_frame, sizeof(syn_frame), tx_out, &tx_len);
    TEST_ASSERT_EQUAL(SYN_OK, st);
    TEST_ASSERT_EQUAL(SYN_TCP_ESTABLISHED, tcp.conns[0].state);
}

void test_tcp_task_unblock_on_data(void)
{
    uint8_t mac[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint32_t ip = 0xA9FE0164;

    syn_eth_init(&eth, mac, ip);
    syn_tcp_init(&tcp, &eth);
    syn_tcp_listen(&tcp, 80);

    tcp.conns[0].state = SYN_TCP_ESTABLISHED;
    tcp.conns[0].remote_ip = 0xA9FE0101;
    tcp.conns[0].remote_port = 49152;
    tcp.conns[0].local_port = 80;

    /* Setup task in SYN_TASK_BLOCKED state */
    task.state = SYN_TASK_BLOCKED;
    tcp.conns[0].blocked_task = &task;

    /* Construct Data frame */
    uint8_t data_frame[64];
    memset(data_frame, 0, sizeof(data_frame));
    data_frame[14] = 0x45;
    data_frame[16] = 0x00;
    data_frame[17] = 50; /* Length 50 (10 payload bytes) */
    data_frame[23] = 6;
    data_frame[26] = 169;
    data_frame[27] = 254;
    data_frame[28] = 1;
    data_frame[29] = 1;
    data_frame[30] = 169;
    data_frame[31] = 254;
    data_frame[32] = 1;
    data_frame[33] = 100;
    data_frame[34] = 0xC0;
    data_frame[35] = 0x00;
    data_frame[36] = 0x00;
    data_frame[37] = 80;
    data_frame[46] = 0x50;
    data_frame[47] = SYN_TCP_FLAG_PSH | SYN_TCP_FLAG_ACK;
    memcpy(&data_frame[54], "GET /ws", 7);

    uint8_t tx_out[128];
    size_t tx_len = 0;

    SYN_Status st = syn_tcp_process_packet(&tcp, data_frame, 64, tx_out, &tx_len);
    TEST_ASSERT_EQUAL(SYN_OK, st);
    TEST_ASSERT_EQUAL(SYN_TASK_READY, task.state); /* Task UNBLOCKED! */
    TEST_ASSERT_EQUAL(10, tcp.conns[0].rx_len);
}

/*
 * Regression: when rx_buf is full, the old code dropped payload bytes but still
 * advanced ack_nxt by the full payload_len — a false ACK.  Verify that ack_nxt
 * only reflects bytes actually accepted into the buffer.
 */
void test_tcp_rx_buf_full_no_false_ack(void)
{
    uint8_t mac[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint32_t ip = 0xA9FE0164;

    syn_eth_init(&eth, mac, ip);
    syn_tcp_init(&tcp, &eth);
    syn_tcp_listen(&tcp, 80);

    tcp.conns[0].state = SYN_TCP_ESTABLISHED;
    tcp.conns[0].remote_ip = 0xA9FE0101;
    tcp.conns[0].remote_port = 49152;
    tcp.conns[0].local_port = 80;

    /* Fill the rx_buf completely */
    tcp.conns[0].rx_len = SYN_TCP_BUF_SIZE;

    /* Build a data frame carrying 5 bytes starting at seq 1000 */
    uint8_t data_frame[64];
    memset(data_frame, 0, sizeof(data_frame));
    data_frame[14] = 0x45;
    data_frame[16] = 0x00;
    data_frame[17] = 45; /* IP len = 20+20+5 */
    data_frame[23] = 6;
    data_frame[26] = 169;
    data_frame[27] = 254;
    data_frame[28] = 1;
    data_frame[29] = 1;
    data_frame[30] = 169;
    data_frame[31] = 254;
    data_frame[32] = 1;
    data_frame[33] = 100;
    data_frame[34] = 0xC0;
    data_frame[35] = 0x00; /* Src port 49152 */
    data_frame[36] = 0x00;
    data_frame[37] = 80; /* Dst port 80 */
    /* seq_num = 1000 */
    data_frame[38] = 0x00;
    data_frame[39] = 0x00;
    data_frame[40] = 0x03;
    data_frame[41] = 0xE8;
    data_frame[46] = 0x50;
    data_frame[47] = SYN_TCP_FLAG_PSH | SYN_TCP_FLAG_ACK;
    memcpy(&data_frame[54], "HELLO", 5);

    uint8_t tx_out[128];
    size_t tx_len = 0;

    SYN_Status st = syn_tcp_process_packet(&tcp, data_frame, 59, tx_out, &tx_len);
    TEST_ASSERT_EQUAL(SYN_OK, st);

    /* Buffer was full — ack_nxt must equal seq (1000), not seq+5 (1005) */
    TEST_ASSERT_EQUAL_UINT32(1000U, tcp.conns[0].ack_nxt);
    /* rx_len must remain at SYN_TCP_BUF_SIZE — nothing was accepted */
    TEST_ASSERT_EQUAL_UINT16((uint16_t)SYN_TCP_BUF_SIZE, tcp.conns[0].rx_len);
}

static void test_tcp_fin_close_handshake(void)
{
    uint8_t mac[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint32_t ip = 0xA9FE0164;

    syn_eth_init(&eth, mac, ip);
    syn_tcp_init(&tcp, &eth);
    syn_tcp_listen(&tcp, 80);

    /* Move connection to ESTABLISHED */
    tcp.conns[0].state = SYN_TCP_ESTABLISHED;
    tcp.conns[0].remote_port = 49152;
    tcp.conns[0].remote_ip = 0xA9FE0101;
    task.state = SYN_TASK_BLOCKED;
    tcp.conns[0].blocked_task = &task;

    /* Build raw FIN frame */
    uint8_t fin_frame[54];
    memset(fin_frame, 0, sizeof(fin_frame));
    memcpy(&fin_frame[0], mac, 6);
    fin_frame[12] = 0x08;
    fin_frame[13] = 0x00;
    fin_frame[14] = 0x45;
    fin_frame[17] = 40;
    fin_frame[23] = 6;
    fin_frame[26] = 169;
    fin_frame[27] = 254;
    fin_frame[28] = 1;
    fin_frame[29] = 1;
    fin_frame[30] = 169;
    fin_frame[31] = 254;
    fin_frame[32] = 1;
    fin_frame[33] = 100;
    fin_frame[34] = 0xC0;
    fin_frame[35] = 0x00;
    fin_frame[36] = 0x00;
    fin_frame[37] = 80;
    fin_frame[38] = 0x00;
    fin_frame[39] = 0x00;
    fin_frame[40] = 0x01;
    fin_frame[41] = 0x00; /* seq = 256 */
    fin_frame[46] = 0x50;
    fin_frame[47] = SYN_TCP_FLAG_FIN;

    uint8_t tx_out[128];
    size_t tx_len = 0;

    SYN_Status st = syn_tcp_process_packet(&tcp, fin_frame, sizeof(fin_frame), tx_out, &tx_len);
    TEST_ASSERT_EQUAL(SYN_OK, st);
    TEST_ASSERT_EQUAL(SYN_TCP_CLOSED, tcp.conns[0].state);
    TEST_ASSERT_EQUAL_UINT32(257, tcp.conns[0].ack_nxt);
    TEST_ASSERT_EQUAL(SYN_TASK_READY, task.state);
}

static void test_tcp_null_params_and_non_tcp_proto(void)
{
    SYN_TCP local_tcp;
    memset(&local_tcp, 0, sizeof(local_tcp));
    uint8_t frame[64] = {0};
    uint8_t tx_out[64];
    size_t tx_len = 0;

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tcp_init(NULL, NULL));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tcp_listen(NULL, 0));

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_tcp_process_packet(NULL, frame, sizeof(frame), tx_out, &tx_len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_tcp_process_packet(&local_tcp, NULL, sizeof(frame), tx_out, &tx_len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_tcp_process_packet(&local_tcp, frame, 50, tx_out, &tx_len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_tcp_process_packet(&local_tcp, frame, sizeof(frame), NULL, &tx_len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_tcp_process_packet(&local_tcp, frame, sizeof(frame), tx_out, NULL));

    /* Non-TCP frame (proto != 6) */
    memset(frame, 0, sizeof(frame));
    frame[23] = 17; /* UDP */
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_tcp_process_packet(&local_tcp, frame, sizeof(frame), tx_out, &tx_len));

    /* Unhandled TCP port -> SYN_ERROR */
    frame[23] = 6;
    frame[36] = 0x1F;
    frame[37] = 0x90; /* Dst Port 8080 */
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_tcp_process_packet(&local_tcp, frame, sizeof(frame), tx_out, &tx_len));

    /* Test odd-length checksum calculation */
    uint8_t odd_data[5] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint16_t csum = syn_tcp_checksum(0xC0A80101, 0xC0A80102, odd_data, 5);
    TEST_ASSERT_NOT_EQUAL(0, csum);

    /* Test max capacity listeners */
    SYN_TCP full_tcp;
    syn_tcp_init(&full_tcp, &eth);
    for (uint16_t p = 1000; p < 1000 + SYN_TCP_MAX_CONNS; p++) {
        TEST_ASSERT_EQUAL(SYN_OK, syn_tcp_listen(&full_tcp, p));
    }
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tcp_listen(&full_tcp, 9999));

    /* Process packet for unmanaged port / listening port without match */
    frame[36] = 0x00;
    frame[37] = 80; /* Managed port 80, but state CLOSED (no match) */
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_tcp_process_packet(&full_tcp, frame, sizeof(frame), tx_out, &tx_len));
}

void run_tcp_tests(void)
{
    RUN_TEST(test_tcp_init_and_listen);
    RUN_TEST(test_tcp_checksum_calc);
    RUN_TEST(test_tcp_syn_ack_handshake);
    RUN_TEST(test_tcp_task_unblock_on_data);
    RUN_TEST(test_tcp_rx_buf_full_no_false_ack);
    RUN_TEST(test_tcp_fin_close_handshake);
    RUN_TEST(test_tcp_null_params_and_non_tcp_proto);
}
