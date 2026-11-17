/* Bare-metal runner: UART stays outside the DW3000 interrupt. */
#include "uwb_runner.h"
#include "uwb_app.h"
#include "uwb_task_stm32.h"
#include "main.h"
#include "dw3000_hw.h"
#include "uwb_port.h"
#include "deca_device_api.h"
#include "dw3000_deca_regs.h"
#include "dwphy.h"
#include "ranging.h"
#include <stdlib.h>

#ifndef UWB_RESPONDER
#define UWB_RESPONDER 0
#endif
#ifndef UWB_ANTENNA_DELAY
#define UWB_ANTENNA_DELAY 16368
#endif

extern UART_HandleTypeDef huart1;
static uint32_t attempt, started_ms, last_ms, last_diag;
static bool pending, running = true;
static char command[16];
static unsigned command_length;

static void emit_result(uint16_t sequence, uint16_t cm, const char *status)
{
    /* Empty diagnostics mean unavailable, not a zero RSSI measurement. */
    printf("TWR,%lu,%lu,%u,%u,%s,%lu\r\n", (unsigned long)HAL_GetTick(),
           (unsigned long)attempt, (unsigned)sequence, (unsigned)cm, status,
           (unsigned long)uwb_task_dropped());
}

static void receive_command(void)
{
    uint8_t ch;
    while (HAL_UART_Receive(&huart1, &ch, 1, 0) == HAL_OK) {
        if (ch == '\r' || ch == '\n') {
            command[command_length] = '\0';
            if (command_length == 1 && command[0] == 's') {
                running = false;
                printf("# PAUSE\r\n");
            } else if (command_length == 1 && command[0] == 'r') {
                if (UWB_RESPONDER) {
                    dwt_forcetrxoff();
                    twr_cancel();
                    dwt_setrxtimeout(0);
                    dwt_setpreambledetecttimeout(0);
                    (void)dwt_rxenable(DWT_START_RX_IMMEDIATE);
                }
                running = true;
                printf("# RUN\r\n");
            } else if (command_length > 1 && command[0] == 'a' &&
                       !pending && !twr_in_progress()) {
                char *end;
                unsigned long delay = strtoul(command + 1, &end, 10);
                if (*end == '\0' && delay <= 65535u) {
                    dwphy_set_antenna_delay((uint16_t)delay);
                    printf("# ANTENNA_DELAY=%lu (volatile; rebuild to retain)\r\n", delay);
                }
            }
            command_length = 0;
        } else if (command_length < sizeof(command) - 1u) {
            command[command_length++] = (char)ch;
        } else {
            command_length = 0;
        }
    }
}

void uwb_runner_init(void)
{
    SystemCoreClockUpdate();
    printf("\r\n# T2 LIBDECA DS-TWR role=%s address=%u peer=%u ant=%u\r\n",
           UWB_RESPONDER ? "responder" : "initiator", UWB_RESPONDER ? 1u : 2u,
           UWB_RESPONDER ? 2u : 1u, (unsigned)UWB_ANTENNA_DELAY);
    if (!uwb_app_init(UWB_RESPONDER ? 1u : 2u, UWB_RESPONDER != 0)) {
        printf("# INIT_FAIL\r\n");
        Error_Handler();
    }
    printf("# READY id=%08lX spi_hz=%lu cpu_hz=%lu\r\n",
           (unsigned long)dwt_readdevid(), (unsigned long)uwb_port_spi_hz(),
           (unsigned long)SystemCoreClock);
    printf("# TWR,tick_ms,attempt,sequence,distance_cm,status,queue_dropped\r\n");
    last_ms = HAL_GetTick();
    last_diag = last_ms;
}

void uwb_runner_poll(void)
{
    (void)uwb_task_poll(1u);
    uwb_range_result_t result;
    if (uwb_app_take_result(&result)) {
        if (UWB_RESPONDER || pending) {
            emit_result(result.sequence, result.distance_cm,
                        result.distance_cm == TWR_FAILED_VALUE ? "FAIL" : "OK");
            pending = false;
        }
    }
    uint32_t now = HAL_GetTick();
    if (pending && (uint32_t)(now - started_ms) >= 1000u) {
        dwt_forcetrxoff();
        twr_cancel();
        emit_result(twr_get_cnum(), TWR_FAILED_VALUE, "TIMEOUT");
        pending = false;
    }
    receive_command();
    if (!UWB_RESPONDER && running && !pending && !twr_in_progress() &&
        (uint32_t)(now - last_ms) >= 100u) {
        last_ms = now;
        started_ms = now;
        ++attempt;
        pending = true;
        /* libdeca can return false after starting its retry path; allow that
         * path to finish and report its callback or our bounded timeout. */
        (void)uwb_app_start_ds_twr(1u);
    }
    if (uwb_task_dropped() != 0u || dw3000_hw_stats.loop_limit_hits != 0u) {
        printf("# FATAL queue_dropped=%lu irq_limit=%lu\r\n",
               (unsigned long)uwb_task_dropped(),
               (unsigned long)dw3000_hw_stats.loop_limit_hits);
        dw3000_hw_interrupt_disable();
        Error_Handler();
    }
    if ((uint32_t)(now - last_diag) >= 5000u && !pending && !twr_in_progress()) {
        last_diag = now;
        uint32_t state = dwt_read_reg(SYS_STATE_LO_ID);
        printf("# HEALTH irq=%d entries=%lu hooks=%lu dropped=%lu state=%08lX\r\n",
               uwb_port_irq_level(), (unsigned long)dw3000_hw_stats.isr_entries,
               (unsigned long)dw3000_hw_stats.hook_calls,
               (unsigned long)uwb_task_dropped(), (unsigned long)state);
    }
}
