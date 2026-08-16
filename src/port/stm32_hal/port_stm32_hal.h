/**
 * @file port_stm32_hal.h
 * @brief STM32 HAL GPIO Port Helper Macros.
 */

#ifndef PORT_STM32_HAL_H
#define PORT_STM32_HAL_H

#include "syntropic/drivers/syn_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Convert STM32 HAL GPIO_TypeDef pointer (GPIOA, GPIOB, etc.) and pin (number 0..15 or mask
 * GPIO_PIN_0..15) into a packed 16-bit SYN_GPIO_Pin handle.
 *
 * Examples:
 *   SYN_PORT_STM32_PIN(GPIOA, 0)                  -> PA0
 *   SYN_PORT_STM32_PIN(GPIOC, GPIO_PIN_13)        -> PC13
 *   SYN_PORT_STM32_PIN(USER_BTN_GPIO_Port, USER_BTN_Pin) -> CubeMX pin
 */
#define SYN_PORT_STM32_PIN(gpio_port, gpio_pin)                                           \
    SYN_GPIO_PIN((uint8_t)(((uintptr_t)(gpio_port) - (uintptr_t)GPIOA) / 0x0400UL),       \
                 (uint8_t)(((uint32_t)(gpio_pin) == 0U) ? 0U                              \
                           : (((uint32_t)(gpio_pin) & ((uint32_t)(gpio_pin) - 1U)) == 0U) \
                               ? (uint32_t)__builtin_ctz((uint32_t)(gpio_pin))            \
                               : (uint32_t)(gpio_pin)))

/**
 * @brief Register an STM32 HAL UART_HandleTypeDef pointer (e.g. &huart1) with a SyntropicOS UART
 * instance (0..5).
 *
 * @param instance  SyntropicOS UART instance index (0..5).
 * @param huart     Pointer to STM32 HAL UART_HandleTypeDef (e.g. &huart1 or &huart3).
 * @return SYN_OK on success, SYN_INVALID_PARAM if instance >= 6.
 */
SYN_Status syn_port_stm32_register_uart(SYN_UARTInstance instance, void *huart);

/**
 * @brief Register an STM32 HAL CRYP_HandleTypeDef pointer (e.g. &hcryp) with SyntropicOS hardware
 * crypto accelerator port.
 *
 * @param hcryp Pointer to STM32 HAL CRYP_HandleTypeDef (or NULL to unregister).
 * @return SYN_OK on success.
 */
SYN_Status syn_port_stm32_register_cryp(void *hcryp);

#ifdef __cplusplus
}
#endif

#endif /* PORT_STM32_HAL_H */
