#ifndef Lum_Assert_H
#define Lum_Assert_H

#include <stdio.h>
#include <stdlib.h>
#include "lum_log.h"

#ifdef Lum_Debug

static inline void lum_assert_fail(const char* expr, const char* file, int line, const char* fn) {
    if (lum_log_is_initialized()) {
        lum_log_message(Lum_Log_Level_Fatal, file, line, fn, "assertion failed: %s", expr);
    } else {
        fprintf(stderr, "[Assert][%s:%d %s] assertion failed: %s\n", file, line, fn, expr);
        fflush(stderr);
    }
    abort();
}

#define Lum_Assert(expr) \
    ((void)((expr) ? (void)0 : lum_assert_fail(#expr, __FILE__, __LINE__, __func__)))
#else

#define Lum_Assert(expr) ((void)0)

#endif

#endif //Lum_Assert_H