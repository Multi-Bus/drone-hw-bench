/* Logging for the br101 platform files and libdeca. Output goes to SEGGER RTT channel 0.
 * Formatted with newlib vsnprintf (libdeca prints 64-bit addresses with PRIX64). */
#ifndef LOG_H
#define LOG_H

#include <inttypes.h> /* libdeca sources use PRIx32 without including it */
#include <stddef.h>
#include <stdint.h>

void t6_log(const char *level, const char *tag, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void t6_log_hexdump(const char *tag, const char *title, const uint8_t *buf, size_t len);

#define LOG_ERR(fmt, ...)  t6_log("E", LOG_TAG, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) t6_log("W", LOG_TAG, fmt, ##__VA_ARGS__)
#define LOG_INF(fmt, ...)  t6_log("I", LOG_TAG, fmt, ##__VA_ARGS__)
#ifdef T6_LOG_DEBUG
#define LOG_DBG(fmt, ...)  t6_log("D", LOG_TAG, fmt, ##__VA_ARGS__)
#else
#define LOG_DBG(fmt, ...)  do { (void)LOG_TAG; } while (0)
#endif
#define LOG_HEXDUMP(title, buf, len) t6_log_hexdump(LOG_TAG, title, (const uint8_t *)(buf), len)

/* libdeca extras, mapped like its Zephyr platform (platform/zephyr/log.h) */
#define DBG_UWB(...)     LOG_DBG(__VA_ARGS__)
#define LOG_INF_IRQ(...) LOG_INF(__VA_ARGS__)
#define LOG_ERR_IRQ(...) LOG_ERR(__VA_ARGS__)
#define DBG_UWB_IRQ(...) /* not logged */

#endif
