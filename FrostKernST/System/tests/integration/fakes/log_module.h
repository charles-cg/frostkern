#ifndef INTEGRATION_LOG_H
#define INTEGRATION_LOG_H
void Integration_Log(const char *, ...);
#define LOG_ERROR_APP(...) Integration_Log(__VA_ARGS__)
#define LOG_WARNING_APP(...) Integration_Log(__VA_ARGS__)
#define LOG_INFO_APP(...) Integration_Log(__VA_ARGS__)
#define LOG_DISPLAY64() "%016llX"
#define LOG_NUMBER64(value) ((unsigned long long)(value))
#endif
