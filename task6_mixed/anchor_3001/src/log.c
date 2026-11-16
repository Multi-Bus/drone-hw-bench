#include <stdarg.h>
#include <stdio.h>

#include "SEGGER_RTT.h"
#include "log.h"

void t6_log(const char *level, const char *tag, const char *fmt, ...)
{
    char line[160];
    int n = snprintf(line, sizeof(line), "%s %s: ", level, tag);
    va_list ap;
    va_start(ap, fmt);
    n += vsnprintf(line + n, sizeof(line) - (size_t)n - 1, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof(line) - 2) {
        n = (int)sizeof(line) - 2;
    }
    line[n++] = '\n';
    SEGGER_RTT_Write(0, line, (unsigned)n);
}

void t6_log_hexdump(const char *tag, const char *title, const uint8_t *buf, size_t len)
{
    t6_log("I", tag, "%s (%u bytes)", title, (unsigned)len);
    for (size_t i = 0; i < len; i++) {
        SEGGER_RTT_printf(0, "%02X%s", buf[i], (i % 16 == 15 || i + 1 == len) ? "\n" : " ");
    }
}
