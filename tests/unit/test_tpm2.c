/**
 * @file test_tpm2.c
 * @brief Unit tests for TCG TPM 2.0 Command Marshaller & Root-of-Trust Engine (syn_tpm2).
 */

#include "mocks/mock_port.h"
#include "syntropic/crypto/syn_tpm2.h"
#include "unity/unity.h"

#include <string.h>

static uint8_t s_tpm_rx_mock[512];
static size_t s_tpm_rx_mock_len = 0U;
static bool s_mock_tpm_send_fail = false;
static bool s_mock_tpm_recv_fail = false;

static bool mock_tpm_send(const uint8_t *data, size_t len, void *ctx)
{
    (void)data;
    (void)len;
    (void)ctx;
    return !s_mock_tpm_send_fail;
}

static bool mock_tpm_recv(uint8_t *buf, size_t max_len, size_t *out_len, void *ctx)
{
    (void)ctx;
    if (s_mock_tpm_recv_fail || s_tpm_rx_mock_len == 0U) {
        return false;
    }
    size_t copy_len = (s_tpm_rx_mock_len < max_len) ? s_tpm_rx_mock_len : max_len;
    (void)memcpy(buf, s_tpm_rx_mock, copy_len);
    *out_len = copy_len;
    return true;
}

static SYN_Transport s_tpm_transport = {.send = mock_tpm_send, .recv = mock_tpm_recv, .ctx = NULL};

static uint8_t s_tpm_tx[256];
static uint8_t s_tpm_rx[256];

/**
 * @brief Helper to write standard TPM 2.0 Success response header into mock RX.
 */
static void set_mock_tpm_success_header(size_t total_len)
{
    s_mock_tpm_send_fail = false;
    s_mock_tpm_recv_fail = false;
    s_tpm_rx_mock[0] = 0x80;
    s_tpm_rx_mock[1] = 0x01; /* Tag: TPM_ST_NO_SESSIONS */
    s_tpm_rx_mock[2] = (uint8_t)((total_len >> 24) & 0xFF);
    s_tpm_rx_mock[3] = (uint8_t)((total_len >> 16) & 0xFF);
    s_tpm_rx_mock[4] = (uint8_t)((total_len >> 8) & 0xFF);
    s_tpm_rx_mock[5] = (uint8_t)(total_len & 0xFF);
    s_tpm_rx_mock[6] = 0x00;
    s_tpm_rx_mock[7] = 0x00;
    s_tpm_rx_mock[8] = 0x00;
    s_tpm_rx_mock[9] = 0x00; /* RC: TPM_RC_SUCCESS */
    s_tpm_rx_mock_len = total_len;
}

void test_tpm2_init_and_lifecycle(void)
{
    SYN_TPM2_Context ctx;
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tpm2_init(NULL, NULL));

    SYN_TPM2_Config cfg = {
        .transport = &s_tpm_transport,
        .rx_buf = s_tpm_rx,
        .rx_buf_size = sizeof(s_tpm_rx),
        .tx_buf = s_tpm_tx,
        .tx_buf_size = sizeof(s_tpm_tx),
    };

    SYN_TPM2_Config bad_cfg = cfg;
    bad_cfg.rx_buf_size = 64U;
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tpm2_init(&ctx, &bad_cfg));

    bad_cfg = cfg;
    bad_cfg.tx_buf_size = 64U;
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tpm2_init(&ctx, &bad_cfg));

    bad_cfg = cfg;
    bad_cfg.transport = NULL;
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tpm2_init(&ctx, &bad_cfg));

    bad_cfg = cfg;
    bad_cfg.rx_buf = NULL;
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tpm2_init(&ctx, &bad_cfg));

    bad_cfg = cfg;
    bad_cfg.tx_buf = NULL;
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tpm2_init(&ctx, &bad_cfg));

    TEST_ASSERT_EQUAL(SYN_OK, syn_tpm2_init(&ctx, &cfg));

    /* Startup */
    set_mock_tpm_success_header(10U);
    TEST_ASSERT_EQUAL(SYN_OK, syn_tpm2_startup(&ctx, SYN_TPM2_SU_CLEAR));
    TEST_ASSERT_EQUAL(SYN_OK, syn_tpm2_startup(&ctx, SYN_TPM2_SU_STATE));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tpm2_startup(NULL, SYN_TPM2_SU_CLEAR));

    SYN_TPM2_Context uninit_ctx;
    (void)memset(&uninit_ctx, 0, sizeof(uninit_ctx));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tpm2_startup(&uninit_ctx, SYN_TPM2_SU_CLEAR));

    /* SelfTest */
    set_mock_tpm_success_header(10U);
    TEST_ASSERT_EQUAL(SYN_OK, syn_tpm2_self_test(&ctx, true));
    TEST_ASSERT_EQUAL(SYN_OK, syn_tpm2_self_test(&ctx, false));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tpm2_self_test(NULL, true));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tpm2_self_test(&uninit_ctx, true));

    /* SelfTest with tight buffer (pos + 1 > max_len) */
    SYN_TPM2_Context small_tx_ctx = ctx;
    small_tx_ctx.cfg.tx_buf_size = 10U;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_self_test(&small_tx_ctx, true));

    /* Transport failure simulation */
    s_mock_tpm_send_fail = true;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_startup(&ctx, SYN_TPM2_SU_CLEAR));

    s_mock_tpm_send_fail = false;
    s_mock_tpm_recv_fail = true;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_startup(&ctx, SYN_TPM2_SU_CLEAR));
}

void test_tpm2_random_and_pcr(void)
{
    SYN_TPM2_Context ctx;
    SYN_TPM2_Config cfg = {
        .transport = &s_tpm_transport,
        .rx_buf = s_tpm_rx,
        .rx_buf_size = sizeof(s_tpm_rx),
        .tx_buf = s_tpm_tx,
        .tx_buf_size = sizeof(s_tpm_tx),
    };
    (void)syn_tpm2_init(&ctx, &cfg);

    /* 1. GetRandom */
    set_mock_tpm_success_header(10U + 2U + 16U);
    s_tpm_rx_mock[10] = 0x00;
    s_tpm_rx_mock[11] = 0x10; /* Length = 16 bytes */
    for (uint8_t i = 0; i < 16; i++) {
        s_tpm_rx_mock[12 + i] = (uint8_t)(i + 1U);
    }

    uint8_t random_bytes[16];
    uint16_t out_len = 0U;
    TEST_ASSERT_EQUAL(SYN_OK, syn_tpm2_get_random(&ctx, 16U, random_bytes, &out_len));
    TEST_ASSERT_EQUAL(16U, out_len);
    TEST_ASSERT_EQUAL(1, random_bytes[0]);

    /* Null & uninitialized validation */
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tpm2_get_random(NULL, 16U, random_bytes, &out_len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tpm2_get_random(&ctx, 16U, NULL, &out_len));
    SYN_TPM2_Context uninit_ctx;
    (void)memset(&uninit_ctx, 0, sizeof(uninit_ctx));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_tpm2_get_random(&uninit_ctx, 16U, random_bytes, &out_len));

    /* GetRandom error execution */
    s_mock_tpm_send_fail = true;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_get_random(&ctx, 16U, random_bytes, &out_len));
    s_mock_tpm_send_fail = false;

    /* GetRandom truncated response */
    set_mock_tpm_success_header(11U); /* Less than 10 + 2 */
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_get_random(&ctx, 16U, random_bytes, &out_len));

    /* GetRandom truncated before full payload (param_size matches buffer, but inner len declares 16
     * bytes) */
    set_mock_tpm_success_header(10U + 2U + 2U);
    s_tpm_rx_mock[10] = 0x00;
    s_tpm_rx_mock[11] = 0x10;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_get_random(&ctx, 16U, random_bytes, &out_len));

    /* 2. PCR Extend SHA-256 and SHA-384 */
    set_mock_tpm_success_header(10U);
    uint8_t dummy_digest[48];
    (void)memset(dummy_digest, 0xAA, sizeof(dummy_digest));
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_tpm2_pcr_extend(&ctx, 0U, SYN_TPM2_ALG_SHA256, dummy_digest, 32U));
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_tpm2_pcr_extend(&ctx, 1U, SYN_TPM2_ALG_SHA384, dummy_digest, 48U));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_tpm2_pcr_extend(&ctx, 24U, SYN_TPM2_ALG_SHA256, dummy_digest, 32U));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_tpm2_pcr_extend(NULL, 0U, SYN_TPM2_ALG_SHA256, dummy_digest, 32U));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_tpm2_pcr_extend(&uninit_ctx, 0U, SYN_TPM2_ALG_SHA256, dummy_digest, 32U));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_tpm2_pcr_extend(&ctx, 0U, SYN_TPM2_ALG_SHA256, NULL, 32U));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_tpm2_pcr_extend(&ctx, 0U, SYN_TPM2_ALG_SHA256, dummy_digest, 0U));

    /* PCR Extend with tight buffers (trigger intermediate write session failures) */
    SYN_TPM2_Context tight_ctx = ctx;
    tight_ctx.cfg.tx_buf_size = 14U;
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_tpm2_pcr_extend(&tight_ctx, 0U, SYN_TPM2_ALG_SHA256, dummy_digest, 32U));
    tight_ctx.cfg.tx_buf_size = 21U;
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_tpm2_pcr_extend(&tight_ctx, 0U, SYN_TPM2_ALG_SHA256, dummy_digest, 32U));
    tight_ctx.cfg.tx_buf_size = 23U;
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_tpm2_pcr_extend(&tight_ctx, 0U, SYN_TPM2_ALG_SHA256, dummy_digest, 32U));
    tight_ctx.cfg.tx_buf_size = 24U;
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_tpm2_pcr_extend(&tight_ctx, 0U, SYN_TPM2_ALG_SHA256, dummy_digest, 32U));
    tight_ctx.cfg.tx_buf_size = 30U;
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_tpm2_pcr_extend(&tight_ctx, 0U, SYN_TPM2_ALG_SHA256, dummy_digest, 32U));

    /* 3. PCR Read SHA-256, SHA-384, PCR 10, PCR 20 */
    set_mock_tpm_success_header(10U + 4U + 4U + (2U + 1U + 3U) + 4U + 2U + 32U);
    size_t p = 10U;
    /* updateCounter */
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 1;
    /* pcrSelection: count = 1 */
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 1;
    /* TPMS_PCR_SELECTION: hashAlg SHA256 (0x000B), sizeOfSelect=3, select[3] */
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0x0B;
    s_tpm_rx_mock[p++] = 3;
    s_tpm_rx_mock[p++] = 0x01;
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0x00;
    /* TPML_DIGEST: count = 1 */
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 1;
    /* TPM2B_DIGEST: size = 32 */
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0x20;
    (void)memset(&s_tpm_rx_mock[p], 0xBB, 32U);

    uint8_t read_digest[48];
    size_t read_len = 0U;
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_tpm2_pcr_read(&ctx, 0U, SYN_TPM2_ALG_SHA256, read_digest, &read_len));
    TEST_ASSERT_EQUAL(32U, read_len);
    TEST_ASSERT_EQUAL(0xBB, read_digest[0]);

    /* PCR Read with NULL out read_len */
    TEST_ASSERT_EQUAL(SYN_OK, syn_tpm2_pcr_read(&ctx, 0U, SYN_TPM2_ALG_SHA256, read_digest, NULL));

    /* Test PCR 10 (index >= 8 && index < 16) and PCR 20 (index >= 16) */
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_tpm2_pcr_read(&ctx, 10U, SYN_TPM2_ALG_SHA256, read_digest, &read_len));
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_tpm2_pcr_read(&ctx, 20U, SYN_TPM2_ALG_SHA256, read_digest, &read_len));

    /* Null validation */
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_tpm2_pcr_read(NULL, 0U, SYN_TPM2_ALG_SHA256, read_digest, &read_len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tpm2_pcr_read(&uninit_ctx, 0U, SYN_TPM2_ALG_SHA256,
                                                           read_digest, &read_len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_tpm2_pcr_read(&ctx, 0U, SYN_TPM2_ALG_SHA256, NULL, &read_len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_tpm2_pcr_read(&ctx, 25U, SYN_TPM2_ALG_SHA256, read_digest, &read_len));

    /* PCR Read with tight buffer (pos + 4 > max_len) */
    tight_ctx.cfg.tx_buf_size = 17U;
    TEST_ASSERT_EQUAL(
        SYN_ERROR, syn_tpm2_pcr_read(&tight_ctx, 0U, SYN_TPM2_ALG_SHA256, read_digest, &read_len));

    /* PCR Read selection parsing boundary failure */
    set_mock_tpm_success_header(16U);
    s_tpm_rx_mock[10] = 0;
    s_tpm_rx_mock[11] = 0;
    s_tpm_rx_mock[12] = 0;
    s_tpm_rx_mock[13] = 1;
    s_tpm_rx_mock[14] = 0;
    s_tpm_rx_mock[15] = 0;
    s_tpm_rx_mock[16] = 0;
    s_tpm_rx_mock[17] = 2; /* 2 selections but buffer ends */
    s_tpm_rx_mock_len = 18U;
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_tpm2_pcr_read(&ctx, 0U, SYN_TPM2_ALG_SHA256, read_digest, &read_len));

    /* PCR Read with 0 digest count */
    set_mock_tpm_success_header(10U + 4U + 4U + (2U + 1U + 3U) + 4U);
    p = 10U;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 1;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 1;
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0x0B;
    s_tpm_rx_mock[p++] = 3;
    s_tpm_rx_mock[p++] = 0x01;
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0x00;
    /* digest count = 0 */
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_tpm2_pcr_read(&ctx, 0U, SYN_TPM2_ALG_SHA256, read_digest, &read_len));

    /* PCR Read with header size 12 (underflow in updateCounter tpm2_read_u32) */
    set_mock_tpm_success_header(12U);
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_tpm2_pcr_read(&ctx, 0U, SYN_TPM2_ALG_SHA256, read_digest, &read_len));

    /* PCR Read truncated before digest bytes (param_size matches buffer, but inner dlen claims 32)
     */
    set_mock_tpm_success_header(10U + 4U + 4U + (2U + 1U + 3U) + 4U + 2U + 4U);
    p = 10U;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 1;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 1;
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0x0B;
    s_tpm_rx_mock[p++] = 3;
    s_tpm_rx_mock[p++] = 0x01;
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 1;
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0x20; /* Declares 32 bytes, but only 4 bytes in buffer */
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_tpm2_pcr_read(&ctx, 0U, SYN_TPM2_ALG_SHA256, read_digest, &read_len));

    /* PCR Read with send error */
    s_mock_tpm_send_fail = true;
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_tpm2_pcr_read(&ctx, 0U, SYN_TPM2_ALG_SHA256, read_digest, &read_len));
    s_mock_tpm_send_fail = false;

    /* PCR Read with NULL out_digest_len */
    set_mock_tpm_success_header(10U + 4U + 4U + (2U + 1U + 3U) + 4U + 2U + 32U);
    p = 10U;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 1;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 1;
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0x0B;
    s_tpm_rx_mock[p++] = 3;
    s_tpm_rx_mock[p++] = 0x01;
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 1;
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0x20;
    TEST_ASSERT_EQUAL(SYN_OK, syn_tpm2_pcr_read(&ctx, 0U, SYN_TPM2_ALG_SHA256, read_digest, NULL));

    /* PCR Read with truncated digest data (triggers tpm2_read_bytes underflow) */
    s_tpm_rx_mock_len = p; /* buffer ends right after digest length */
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_tpm2_pcr_read(&ctx, 0U, SYN_TPM2_ALG_SHA256, read_digest, &read_len));
}

void test_tpm2_quote_and_nvram(void)
{
    SYN_TPM2_Context ctx;
    SYN_TPM2_Config cfg = {
        .transport = &s_tpm_transport,
        .rx_buf = s_tpm_rx,
        .rx_buf_size = sizeof(s_tpm_rx),
        .tx_buf = s_tpm_tx,
        .tx_buf_size = sizeof(s_tpm_tx),
    };
    (void)syn_tpm2_init(&ctx, &cfg);

    /* 1. Quote */
    set_mock_tpm_success_header(10U + 4U + (2U + 8U) + (2U + 2U + 8U));
    size_t p = 10U;
    /* parameterSize */
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 20;
    /* TPM2B_ATTEST quoted */
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0x08;
    (void)memset(&s_tpm_rx_mock[p], 0x11, 8U);
    p += 8U;
    /* TPMT_SIGNATURE */
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0x14; /* RSASSA */
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0x08;
    (void)memset(&s_tpm_rx_mock[p], 0x22, 8U);

    SYN_TPM2_QuoteResult quote;
    uint8_t nonce[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    TEST_ASSERT_EQUAL(SYN_OK, syn_tpm2_quote(&ctx, 0x81010001U, nonce, 8U, 0x01U, &quote));
    TEST_ASSERT_EQUAL(8U, quote.attest_len);
    TEST_ASSERT_EQUAL(8U, quote.signature_len);

    /* Quote without nonce (NULL / 0) */
    TEST_ASSERT_EQUAL(SYN_OK, syn_tpm2_quote(&ctx, 0x81010001U, NULL, 0U, 0x01U, &quote));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_tpm2_quote(NULL, 0x81010001U, NULL, 0U, 0x01U, &quote));
    SYN_TPM2_Context uninit_ctx;
    (void)memset(&uninit_ctx, 0, sizeof(uninit_ctx));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_tpm2_quote(&uninit_ctx, 0x81010001U, NULL, 0U, 0x01U, &quote));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_tpm2_quote(&ctx, 0x81010001U, NULL, 5U, 0x01U, &quote));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tpm2_quote(&ctx, 0x81010001U, nonce, 8U, 0x01U, NULL));

    /* Quote with tight buffer (trigger write session failure) */
    SYN_TPM2_Context tight_ctx = ctx;
    tight_ctx.cfg.tx_buf_size = 14U;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_quote(&tight_ctx, 0x81010001U, nonce, 8U, 0x01U, &quote));
    tight_ctx.cfg.tx_buf_size = 28U;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_quote(&tight_ctx, 0x81010001U, nonce, 8U, 0x01U, &quote));
    tight_ctx.cfg.tx_buf_size = 35U;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_quote(&tight_ctx, 0x81010001U, nonce, 8U, 0x01U, &quote));
    tight_ctx.cfg.tx_buf_size = 37U;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_quote(&tight_ctx, 0x81010001U, nonce, 8U, 0x01U, &quote));
    tight_ctx.cfg.tx_buf_size = 39U;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_quote(&tight_ctx, 0x81010001U, nonce, 8U, 0x01U, &quote));
    tight_ctx.cfg.tx_buf_size = 46U; /* triggers pos + 4U > max_len in pcr_selection */
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_quote(&tight_ctx, 0x81010001U, nonce, 8U, 0x01U, &quote));

    /* Quote oversize attest length error */
    set_mock_tpm_success_header(10U + 4U + 2U);
    s_tpm_rx_mock[14] = 0x02; /* 512 bytes > SYN_TPM2_MAX_QUOTE_LEN */
    s_tpm_rx_mock[15] = 0x00;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_quote(&ctx, 0x81010001U, nonce, 8U, 0x01U, &quote));

    /* Quote oversize sig length error */
    set_mock_tpm_success_header(10U + 4U + (2U + 4U) + (2U + 2U));
    p = 10U;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 20;
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0x04;
    p += 4U;
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0x14;
    s_tpm_rx_mock[p++] = 0x02; /* 512 bytes > SYN_TPM2_MAX_QUOTE_LEN */
    s_tpm_rx_mock[p++] = 0x00;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_quote(&ctx, 0x81010001U, nonce, 8U, 0x01U, &quote));

    /* Quote with truncated signature data */
    set_mock_tpm_success_header(10U + 4U + (2U + 4U) + (2U + 2U + 8U));
    p = 10U;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 20;
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0x04;
    p += 4U;
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0x14;
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0x08;
    s_tpm_rx_mock_len = p; /* truncated before 8 signature bytes */
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_quote(&ctx, 0x81010001U, nonce, 8U, 0x01U, &quote));

    /* Quote with send error */
    s_mock_tpm_send_fail = true;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_quote(&ctx, 0x81010001U, nonce, 8U, 0x01U, &quote));
    s_mock_tpm_send_fail = false;

    /* 2. NV Write & NV Read */
    set_mock_tpm_success_header(10U);
    uint8_t nv_data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_tpm2_nv_write(&ctx, SYN_TPM2_RH_OWNER, 0x01500000U, 0U, nv_data, 4U));

    /* NV Write with send error */
    s_mock_tpm_send_fail = true;
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_tpm2_nv_write(&ctx, SYN_TPM2_RH_OWNER, 0x01500000U, 0U, nv_data, 4U));
    s_mock_tpm_send_fail = false;

    set_mock_tpm_success_header(10U + 4U + 2U + 4U);
    p = 10U;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 6;
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0x04;
    (void)memcpy(&s_tpm_rx_mock[p], nv_data, 4U);

    uint8_t read_nv[4];
    uint16_t read_nv_len = 0U;
    TEST_ASSERT_EQUAL(SYN_OK, syn_tpm2_nv_read(&ctx, SYN_TPM2_RH_OWNER, 0x01500000U, 0U, 4U,
                                               read_nv, &read_nv_len));
    TEST_ASSERT_EQUAL(4U, read_nv_len);
    TEST_ASSERT_EQUAL(0xDE, read_nv[0]);

    /* NV Read with truncated data */
    s_tpm_rx_mock_len = 16U; /* truncated before 4 data bytes */
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_nv_read(&ctx, SYN_TPM2_RH_OWNER, 0x01500000U, 0U, 4U,
                                                  read_nv, &read_nv_len));

    /* NV Read with NULL out_len */
    set_mock_tpm_success_header(10U + 4U + 2U + 4U);
    p = 10U;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 6;
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0x04;
    (void)memcpy(&s_tpm_rx_mock[p], nv_data, 4U);
    TEST_ASSERT_EQUAL(
        SYN_OK, syn_tpm2_nv_read(&ctx, SYN_TPM2_RH_OWNER, 0x01500000U, 0U, 4U, read_nv, NULL));

    /* NV Read with send error */
    s_mock_tpm_send_fail = true;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_nv_read(&ctx, SYN_TPM2_RH_OWNER, 0x01500000U, 0U, 4U,
                                                  read_nv, &read_nv_len));
    s_mock_tpm_send_fail = false;

    /* Null validation */
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tpm2_nv_write(NULL, 0, 0, 0, NULL, 0));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tpm2_nv_write(&uninit_ctx, 0, 0, 0, nv_data, 4));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tpm2_nv_write(&ctx, 0, 0, 0, NULL, 4));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tpm2_nv_write(&ctx, 0, 0, 0, nv_data, 0));

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tpm2_nv_read(NULL, 0, 0, 0, 0, NULL, NULL));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tpm2_nv_read(&uninit_ctx, 0, 0, 0, 4, read_nv, NULL));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tpm2_nv_read(&ctx, 0, 0, 0, 4, NULL, NULL));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_tpm2_nv_read(&ctx, 0, 0, 0, 0, read_nv, NULL));

    /* NV Write & NV Read with tight buffer */
    tight_ctx.cfg.tx_buf_size = 14U;
    TEST_ASSERT_EQUAL(
        SYN_ERROR, syn_tpm2_nv_write(&tight_ctx, SYN_TPM2_RH_OWNER, 0x01500000U, 0U, nv_data, 4U));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_nv_read(&tight_ctx, SYN_TPM2_RH_OWNER, 0x01500000U, 0U,
                                                  4U, read_nv, &read_nv_len));
    tight_ctx.cfg.tx_buf_size = 28U;
    TEST_ASSERT_EQUAL(
        SYN_ERROR, syn_tpm2_nv_write(&tight_ctx, SYN_TPM2_RH_OWNER, 0x01500000U, 0U, nv_data, 4U));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_nv_read(&tight_ctx, SYN_TPM2_RH_OWNER, 0x01500000U, 0U,
                                                  4U, read_nv, &read_nv_len));
    tight_ctx.cfg.tx_buf_size = 30U;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_nv_read(&tight_ctx, SYN_TPM2_RH_OWNER, 0x01500000U, 0U,
                                                  4U, read_nv, &read_nv_len));
    tight_ctx.cfg.tx_buf_size = 32U;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_nv_read(&tight_ctx, SYN_TPM2_RH_OWNER, 0x01500000U, 0U,
                                                  4U, read_nv, &read_nv_len));
    TEST_ASSERT_EQUAL(
        SYN_ERROR, syn_tpm2_nv_write(&tight_ctx, SYN_TPM2_RH_OWNER, 0x01500000U, 0U, nv_data, 4U));
    tight_ctx.cfg.tx_buf_size = 34U;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_nv_read(&tight_ctx, SYN_TPM2_RH_OWNER, 0x01500000U, 0U,
                                                  4U, read_nv, &read_nv_len));
    TEST_ASSERT_EQUAL(
        SYN_ERROR, syn_tpm2_nv_write(&tight_ctx, SYN_TPM2_RH_OWNER, 0x01500000U, 0U, nv_data, 4U));
    tight_ctx.cfg.tx_buf_size = 38U;
    TEST_ASSERT_EQUAL(
        SYN_ERROR, syn_tpm2_nv_write(&tight_ctx, SYN_TPM2_RH_OWNER, 0x01500000U, 0U, nv_data, 4U));

    /* NV Read truncated before data payload (param_size matches, but inner len declares 4) */
    set_mock_tpm_success_header(10U + 4U + 2U + 1U);
    p = 10U;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 0;
    s_tpm_rx_mock[p++] = 6;
    s_tpm_rx_mock[p++] = 0x00;
    s_tpm_rx_mock[p++] = 0x04; /* Declares 4 bytes, only 1 byte present */
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_nv_read(&ctx, SYN_TPM2_RH_OWNER, 0x01500000U, 0U, 4U,
                                                  read_nv, &read_nv_len));

    /* TPM Error Code Simulation */
    s_tpm_rx_mock[9] = 0x01; /* TPM_RC_FAILURE */
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_startup(&ctx, SYN_TPM2_SU_CLEAR));
    TEST_ASSERT_EQUAL(0x01U, syn_tpm2_get_last_rc(&ctx));
    TEST_ASSERT_EQUAL(0U, syn_tpm2_get_last_rc(NULL));

    /* Truncated response: param_size in header claims 50 bytes, but RX len is only 10 */
    set_mock_tpm_success_header(50U);
    s_tpm_rx_mock_len = 10U; /* Truncated buffer */
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_startup(&ctx, SYN_TPM2_SU_CLEAR));

    /* Bad response tag */
    set_mock_tpm_success_header(10U);
    s_tpm_rx_mock[1] = 0xFF; /* Unknown tag */
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_tpm2_startup(&ctx, SYN_TPM2_SU_CLEAR));

    /* SESSIONS response tag */
    set_mock_tpm_success_header(10U);
    s_tpm_rx_mock[1] = 0x02; /* SYN_TPM2_ST_SESSIONS */
    TEST_ASSERT_EQUAL(SYN_OK, syn_tpm2_startup(&ctx, SYN_TPM2_SU_CLEAR));
}

void run_tpm2_tests(void)
{
    RUN_TEST(test_tpm2_init_and_lifecycle);
    RUN_TEST(test_tpm2_random_and_pcr);
    RUN_TEST(test_tpm2_quote_and_nvram);
}
