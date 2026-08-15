#include "mock_port.h"
#include "syntropic/log/syn_log.h"
#include "syntropic/net/syn_mqtt.h"
#include "unity/unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool message_received = false;
static char last_payload[128] = {0};

static void on_mqtt_msg(const char *topic, const uint8_t *payload, size_t len, void *ctx)
{
    (void)ctx;
    printf("[Integration Test] Received MQTT message on topic '%s': %.*s\n", topic, (int)len,
           (char *)payload);
    if (len < sizeof(last_payload)) {
        memcpy(last_payload, payload, len);
        last_payload[len] = '\0';
    }
    message_received = true;
}

void setUp(void)
{
}
void tearDown(void)
{
}

void test_mqtt_mosquitto_e2e(void)
{
    const char *host = getenv("MQTT_HOST");
    if (!host)
        host = "127.0.0.1";
    uint16_t port = 1883;

    uint8_t rx_buf[256];
    uint8_t tx_buf[256];
    SYN_MqttClient client;

    printf("[Integration Test] Connecting to Mosquitto Broker at %s:%d...\n", host, port);
    SYN_Status status = syn_mqtt_init(&client, host, port, "syn_integration_test_client", NULL,
                                      NULL, 60, rx_buf, sizeof(rx_buf), tx_buf, sizeof(tx_buf));
    TEST_ASSERT_EQUAL_INT(SYN_OK, status);

    client.on_message = on_mqtt_msg;

    SYN_PT pt;
    PT_INIT(&pt);

    SYN_Task task;
    memset(&task, 0, sizeof(task));
    task.user_data = &client;

    /* Drive task loop until connected */
    int iterations = 0;
    while (client.state != SYN_MQTT_CONNECTED && iterations < 200) {
        mock_tick_advance(10);
        syn_mqtt_task(&pt, &task);
        usleep(10000); /* 10ms */
        iterations++;
    }
    if (client.state != SYN_MQTT_CONNECTED) {
        printf("[Integration Test] Notice: Mosquitto broker at %s:%d not reachable (skipping "
               "loopback test)\n",
               host, port);
        return;
    }
    printf("[Integration Test] Connected to Mosquitto Broker!\n");

    /* Subscribe to syntropic/test */
    status = syn_mqtt_subscribe(&client, "syntropic/test", 0);
    TEST_ASSERT_EQUAL_INT(SYN_OK, status);

    /* Drive task loop to process SUBACK */
    for (int i = 0; i < 20; i++) {
        mock_tick_advance(10);
        syn_mqtt_task(&pt, &task);
        usleep(10000);
    }

    /* Publish message to syntropic/test */
    const char *test_msg = "Hello Mosquitto from SyntropicOS!";
    status = syn_mqtt_publish(&client, "syntropic/test", (const uint8_t *)test_msg,
                              strlen(test_msg), 0, false);
    TEST_ASSERT_EQUAL_INT(SYN_OK, status);

    /* Drive task loop to process incoming published message */
    iterations = 0;
    while (!message_received && iterations < 100) {
        mock_tick_advance(10);
        syn_mqtt_task(&pt, &task);
        usleep(10000);
        iterations++;
    }

    TEST_ASSERT_TRUE(message_received);
    TEST_ASSERT_EQUAL_STRING(test_msg, last_payload);

    /* Publish message with QoS 1 */
    message_received = false;
    memset(last_payload, 0, sizeof(last_payload));
    const char *qos1_msg = "QoS 1 Message from SyntropicOS!";
    status = syn_mqtt_publish(&client, "syntropic/test", (const uint8_t *)qos1_msg,
                              strlen(qos1_msg), 1, false);
    TEST_ASSERT_EQUAL_INT(SYN_OK, status);

    iterations = 0;
    while (!message_received && iterations < 100) {
        mock_tick_advance(10);
        syn_mqtt_task(&pt, &task);
        usleep(10000);
        iterations++;
    }
    TEST_ASSERT_TRUE(message_received);
    TEST_ASSERT_EQUAL_STRING(qos1_msg, last_payload);

    /* Manual PINGREQ test */
    status = syn_mqtt_ping(&client);
    TEST_ASSERT_EQUAL_INT(SYN_OK, status);
    for (int i = 0; i < 10; i++) {
        mock_tick_advance(10);
        syn_mqtt_task(&pt, &task);
        usleep(10000);
    }

    /* Clean Disconnect */
    syn_mqtt_disconnect(&client);
    for (int i = 0; i < 5; i++) {
        mock_tick_advance(10);
        syn_mqtt_task(&pt, &task);
        usleep(10000);
    }
    TEST_ASSERT_EQUAL_INT(SYN_MQTT_DISCONNECTED, client.state);

    printf("[Integration Test] End-to-End Mosquitto Integration PASS!\n");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_mqtt_mosquitto_e2e);
    return UNITY_END();
}
