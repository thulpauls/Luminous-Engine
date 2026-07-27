#ifndef Lum_Log_H
#define Lum_Log_H

#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>

typedef enum lum_Log_level {
    Lum_Log_Level_Trace,
    Lum_Log_Level_Debug,
    Lum_Log_Level_Info,
    Lum_Log_Level_Warn,
    Lum_Log_Level_Error,
    Lum_Log_Level_Fatal
} lum_Log_level;

int lum_log_init(void);
void lum_log_shutdown(void);
bool lum_log_is_initialized(void);

void lum_log_set_min_level(lum_Log_level level);
void lum_log_enable_color(bool enable);
void lum_log_set_file(FILE* file);

void lum_log_message(lum_Log_level level, const char* file, int line, const char* fn, const char* fmt, ...);

#ifdef Lum_Debug
    #define Lum_Log_Trace(...) lum_log_message(Lum_Log_Level_Trace, __FILE__, __LINE__, __func__, __VA_ARGS__)
    #define Lum_Log_Debug(...) lum_log_message(Lum_Log_Level_Debug, __FILE__, __LINE__, __func__, __VA_ARGS__)
#else
    #define Lum_Log_Trace(...) ((void)0)
    #define Lum_Log_Debug(...) ((void)0)
#endif
#define Lum_Log_Info(...) lum_log_message(Lum_Log_Level_Info, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define Lum_Log_Warn(...) lum_log_message(Lum_Log_Level_Warn, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define Lum_Log_Error(...) lum_log_message(Lum_Log_Level_Error, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define Lum_Log_Fatal(...) lum_log_message(Lum_Log_Level_Fatal, __FILE__, __LINE__, __func__, __VA_ARGS__)

#endif //Lum_Log_H