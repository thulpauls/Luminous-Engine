#include "lum_log.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <io.h>
    #define Lum_Is_Tty(stream) (_isatty(_fileno(stream)) != 0)
#else
    #include <unistd.h>
    #define Lum_Is_Tty(stream) (isatty(fileno(stream)) != 0)
#endif

typedef struct lum_Log {
    bool initialized;
    lum_Log_level min_level;
    bool use_color;
    bool output_stderr;
    FILE* file;
} lum_Log;

static lum_Log g_log;

static void lum_log_reset(void) {
    memset(&g_log, 0, sizeof(g_log));
}

static const char* lum__log_level_get_name(lum_Log_level level) {
    switch (level) {
        case Lum_Log_Level_Trace: return "Trace";
        case Lum_Log_Level_Debug: return "Debug";
        case Lum_Log_Level_Info: return "Info";
        case Lum_Log_Level_Warn: return "Warn";
        case Lum_Log_Level_Error: return "Error";
        case Lum_Log_Level_Fatal: return "Fatal";
        default: return "Unknown";
    }
}

static const char* lum__log_level_get_color(lum_Log_level level) {
    switch (level) {
        case Lum_Log_Level_Trace: return "\x1b[90m";
        case Lum_Log_Level_Debug: return "\x1b[36m";
        case Lum_Log_Level_Info: return "\x1b[32m";
        case Lum_Log_Level_Warn: return "\x1b[33m";
        case Lum_Log_Level_Error: return "\x1b[31m";
        case Lum_Log_Level_Fatal: return "\x1b[35m";
        default: return "";
    }
}

int lum_log_init(void) {
    lum_log_reset();

    g_log.min_level = Lum_Log_Level_Debug;
    g_log.use_color = Lum_Is_Tty(stderr);
    g_log.output_stderr = true;
    g_log.file = NULL;

    g_log.initialized = true;
    return 1;
}

void lum_log_shutdown(void) {
    if (!g_log.initialized) return;
    if (g_log.file) {
        fclose(g_log.file);
        g_log.file = NULL;
    }
    lum_log_reset();
}

void lum_log_message(lum_Log_level level, const char* file, int line, const char* fn, const char* fmt, ...) {
    if (!g_log.initialized) return;
    if (level < g_log.min_level) return;

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_buf[16];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);

    const char* reset = g_log.use_color ? "\x1b[0m" : "";
    const char* level_color = g_log.use_color ? lum__log_level_get_color(level) : "";
    const char* level_name = lum__log_level_get_name(level);

    va_list args;
    va_start(args, fmt);

    if (g_log.output_stderr) {
        fprintf(stderr, "%s[%s][%s][%s:%d %s]%s ", level_color, time_buf, level_name, file, line, fn, reset);
        va_list args_copy;
        va_copy(args_copy, args);
        vfprintf(stderr, fmt, args_copy);
        va_end(args_copy);
        fprintf(stderr, "\n");
        fflush(stderr);
    }

    if (g_log.file) {
        fprintf(g_log.file, "%s[%s][%s][%s:%d %s]%s ", time_buf, level_name, file, line, fn, reset);
        va_list args_copy;
        va_copy(args_copy, args);
        vfprintf(g_log.file, fmt, args_copy);
        va_end(args_copy);
        fprintf(g_log.file, "\n");
        fflush(g_log.file);
    }

    va_end(args);
}

void lum_log_set_min_level(lum_Log_level level) {
    if (!g_log.initialized) return;
    g_log.min_level = level;
}

void lum_log_enable_color(bool enable) {
    if (!g_log.initialized) return;
    g_log.use_color = enable;
}

void lum_log_set_file(FILE* file) {
    if (!g_log.initialized) return;
    g_log.file = file;
}