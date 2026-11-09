#ifndef TASK2_UWB_MINIMAL_LOG_H
#define TASK2_UWB_MINIMAL_LOG_H

/* Temporary libdeca log shim; no printf from IRQ or TWR timing-critical code.
 * Actual distance output belongs in the main application loop.
 * Set UWB_VERBOSE_LOGS=1 only to diagnose non-IRQ failures. */
#ifndef UWB_VERBOSE_LOGS
#define UWB_VERBOSE_LOGS 0
#endif

#if UWB_VERBOSE_LOGS
#include <stdio.h>
#define LOG_ERR(...) do { printf(__VA_ARGS__); printf("\r\n"); } while (0)
#define LOG_WARN(...) LOG_ERR(__VA_ARGS__)
#define LOG_INF(...) LOG_ERR(__VA_ARGS__)
#define LOG_DBG(...) do {} while (0)
#else
#define LOG_ERR(...) do {} while (0)
#define LOG_WARN(...) do {} while (0)
#define LOG_INF(...) do {} while (0)
#define LOG_DBG(...) do {} while (0)
#endif

#define LOG_ERR_IRQ(...) do {} while (0)
#define LOG_INF_IRQ(...) do {} while (0)
#define LOG_WARN_IRQ(...) do {} while (0)
#define LOG_DBG_IRQ(...) do {} while (0)
#define LOG_HEXDUMP(...) do {} while (0)
#define LOG_ERR_TS(...) do {} while (0)
#define LOG_INF_TS(...) do {} while (0)
#define LOG_DBG_TS(...) do {} while (0)
#define LOG_DBGL_TS(...) do {} while (0)
#define DBG_UWB(...) do {} while (0)
#define DBG_UWB_IRQ(...) do {} while (0)

#endif
