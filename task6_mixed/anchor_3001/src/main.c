/* Task 6 Lv1 step 1: read the DW3110 Device ID on a DWM3001CDK with the br101 driver v08.02.02.
 * Output: SEGGER RTT channel 0 (J-Link RTT Viewer).
 * Sequence follows Task 2 T2_UWB_ISR: driver register reads only after dwt_initialise(). */
#include <stdint.h>

#include "SEGGER_RTT.h"
#include "nrf_delay.h"

#include "deca_device_api.h"
#include "deca_probe_interface.h"
#include "deca_version.h"
#include "dw3000_hw.h"
#include "dw3000_spi.h"

#define EXPECTED_ID 0xDECA0302UL
#define RAW_READS   20

/* DEV_ID (register file 0, offset 0) read with a 1-byte header, without the driver. */
static uint32_t raw_dev_id(void)
{
    uint8_t header = 0x00;
    uint8_t b[4] = {0};
    if (dw3000_spi_read(1, &header, 4, b) != DWT_SUCCESS) {
        return 0;
    }
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

int main(void)
{
    SEGGER_RTT_Init();
    SEGGER_RTT_printf(0, "\n== T6 Lv1: DWM3001CDK Device ID (br101 driver %s) ==\n", DRIVER_VERSION_STR);

    int rc = dw3000_hw_init();
    SEGGER_RTT_printf(0, "dw3000_hw_init: %d (0 = RSTn released and SPI ready)\n", rc);

    unsigned ok = 0;
    for (unsigned i = 1; i <= RAW_READS; i++) {
        dw3000_hw_reset();
        nrf_delay_ms(5);
        uint32_t id = raw_dev_id();
        if (id == EXPECTED_ID) {
            ok++;
        }
        SEGGER_RTT_printf(0, "#%02u raw id=0x%08X %s\n", i, (unsigned)id, id == EXPECTED_ID ? "OK" : "NG");
    }
    SEGGER_RTT_printf(0, "raw reads: %u/%u = 0x%08X\n", ok, RAW_READS, (unsigned)EXPECTED_ID);

    dw3000_hw_reset();
    nrf_delay_ms(5);
    rc = dwt_probe((struct dwt_probe_s *)&dw3000_probe_interf);
    SEGGER_RTT_printf(0, "dwt_probe: %d (DWT_SUCCESS = %d)\n", rc, DWT_SUCCESS);
    rc = dwt_initialise(DWT_DW_INIT);
    SEGGER_RTT_printf(0, "dwt_initialise: %d\n", rc);
    uint32_t id = dwt_readdevid();
    SEGGER_RTT_printf(0, "dwt_readdevid: 0x%08X %s\n", (unsigned)id, id == EXPECTED_ID ? "OK" : "NG");

    int pass = (ok == RAW_READS) && (rc == DWT_SUCCESS) && (id == EXPECTED_ID);
    SEGGER_RTT_printf(0, "verdict: %s\n== done ==\n", pass ? "PASS" : "FAIL");

    for (;;) {
        __WFE();
    }
}
