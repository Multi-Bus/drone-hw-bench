/* DWM3001CDK pin map for the br101 nRF5 SDK platform port.
 * Source: Qorvo DW3_QM33_SDK 1.1.1, Projects/FreeRTOS/CLI/DWM3001CDK/ProjectDefinition/uwb_stack_llhw.cmake
 * Pin number = port * 32 + pin. */
#ifndef CONFIG_H
#define CONFIG_H

#define CONFIG_DW3000_SPI_CLK     3   /* P0.03 */
#define CONFIG_DW3000_SPI_MOSI    8   /* P0.08 */
#define CONFIG_DW3000_SPI_MISO    29  /* P0.29 */
#define CONFIG_DW3000_SPI_CS      38  /* P1.06 */
#define CONFIG_DW3000_GPIO_IRQ    34  /* P1.02 */
#define CONFIG_DW3000_GPIO_RESET  25  /* P0.25 */
/* The SDK lists WAKEUP at P1.19, but nRF52833 port 1 only has P1.00-P1.09.
 * -1 makes the port wake the chip with the SPI CS line instead. */
#define CONFIG_DW3000_GPIO_WAKEUP (-1)
#define CONFIG_DW3000_SPI_MAX_MHZ 8

#endif
