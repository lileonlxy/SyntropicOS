/**
 * @file    main.c
 * @brief   SyntropicOS Example — STM32 Secure USART Reception with SHA-256 and AES-128
 *
 * Demonstrates:
 *   1. Interrupt-driven UART RX buffering via syn_ringbuf.
 *   2. SHA-256 cryptographic hashing (syn_sha256).
 *   3. AES-128-CBC encryption/decryption with PKCS#7 padding (syn_aes128).
 *
 * Hardware:
 *   - Board: STM32F4 / STM32F1 Nucleo / Discovery
 *   - UART:  USART2 (115200 8N1)
 *   - Pins:  PA2 (TX), PA3 (RX)
 */

#include <stdio.h>
#include "stm32f4xx_hal.h"
#include "syntropic/syntropic.h"
#include "syntropic/util/syn_ringbuf.h"
#include "syntropic/crypto/syn_sha256.h"
#include "syntropic/crypto/syn_aes128.h"
#include "port/stm32_hal/port_stm32_hal.h"

#define RX_BUF_SIZE 128

static uint8_t rx_backing_buf[RX_BUF_SIZE];
static SYN_RingBuf rx_ringbuf;

static UART_HandleTypeDef huart2;
static uint8_t rx_byte;

/* Secret 128-bit AES Key and Initialization Vector (IV) */
static const uint8_t aes_key[16] = {
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
};

static const uint8_t aes_iv[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void print_hex(const char *label, const uint8_t *data, size_t len);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    /* 1. Initialize Ring Buffer */
    syn_ringbuf_init(&rx_ringbuf, rx_backing_buf, sizeof(rx_backing_buf));

    /* 2. Register UART handle & start IT reception */
    syn_port_stm32_register_uart(0, &huart2);
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);

    /* Welcome message */
    const char *msg = "\r\nSyntropicOS STM32 SHA-256 + AES-128 USART Example Ready!\r\nType text and press ENTER:\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)strlen(msg), 1000);

    uint8_t byte;
    uint8_t line_buf[64];
    size_t line_len = 0;

    SYN_AES128_Context aes_ctx;
    syn_aes128_init(&aes_ctx, aes_key);

    /* 3. Main Loop */
    while (1)
    {
        while (syn_ringbuf_get(&rx_ringbuf, &byte))
        {
            /* Echo input character */
            HAL_UART_Transmit(&huart2, &byte, 1, 10);

            if (byte == '\r' || byte == '\n')
            {
                if (line_len > 0)
                {
                    line_buf[line_len] = '\0';
                    const char *nl = "\r\n--- Cryptographic Processing ---\r\n";
                    HAL_UART_Transmit(&huart2, (uint8_t *)nl, (uint16_t)strlen(nl), 100);

                    /* A. Compute SHA-256 Hash */
                    uint8_t sha256_hash[32];
                    syn_sha256(line_buf, line_len, sha256_hash);
                    print_hex("SHA-256 Digest", sha256_hash, sizeof(sha256_hash));

                    /* B. Encrypt with AES-128-CBC */
                    uint8_t ciphertext[80];
                    size_t cipher_len = 0;
                    if (syn_aes128_cbc_encrypt(&aes_ctx, aes_iv, line_buf, line_len, ciphertext, sizeof(ciphertext), &cipher_len) == SYN_OK)
                    {
                        print_hex("AES-128 Ciphertext", ciphertext, cipher_len);

                        /* C. Decrypt with AES-128-CBC to verify */
                        uint8_t decrypted[80];
                        size_t plain_len = 0;
                        if (syn_aes128_cbc_decrypt(&aes_ctx, aes_iv, ciphertext, cipher_len, decrypted, sizeof(decrypted), &plain_len) == SYN_OK)
                        {
                            decrypted[plain_len] = '\0';
                            char buf[128];
                            int len = snprintf(buf, sizeof(buf), "Decrypted Plaintext: \"%s\"\r\n\r\n", decrypted);
                            if (len > 0)
                            {
                                HAL_UART_Transmit(&huart2, (uint8_t *)buf, (uint16_t)len, 100);
                            }
                        }
                    }

                    line_len = 0;
                }
            }
            else if (line_len < sizeof(line_buf) - 1)
            {
                line_buf[line_len++] = byte;
            }
        }

        HAL_Delay(5);
    }
}

static void print_hex(const char *label, const uint8_t *data, size_t len)
{
    char line[256];
    int pos = snprintf(line, sizeof(line), "%s: ", label);
    for (size_t i = 0; i < len && pos < (int)sizeof(line) - 3; i++)
    {
        pos += snprintf(line + pos, sizeof(line) - pos, "%02X", data[i]);
    }
    snprintf(line + pos, sizeof(line) - pos, "\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)line, (uint16_t)strlen(line), 200);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        syn_ringbuf_put(&rx_ringbuf, rx_byte);
        HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 16;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}
