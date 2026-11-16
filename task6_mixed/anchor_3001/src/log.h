/* Logging used by the br101 platform files. Output goes to SEGGER RTT channel 0. */
#ifndef LOG_H
#define LOG_H

#include "SEGGER_RTT.h"

#define LOG_ERR(fmt, ...) SEGGER_RTT_printf(0, "E %s: " fmt "\n", LOG_TAG, ##__VA_ARGS__)
#define LOG_INF(fmt, ...) SEGGER_RTT_printf(0, "I %s: " fmt "\n", LOG_TAG, ##__VA_ARGS__)

#endif
