#ifndef __ERSWITCHER_PORTS_LOGGER__H__
#define __ERSWITCHER_PORTS_LOGGER__H__

/*
 * Отладочный вывод
 *
 * Включается переменной окружения ERSWITCHER_DEBUG=1. Вывод идёт в stderr
 * с монотонной временной меткой в секундах от запуска приложения
 */

extern int ERSWITCHER_DEBUG;

void logger_init(void);
void logger_debug(const char* format, ...);

#define DEBUG_LOG(...)                 \
    do {                               \
        if(ERSWITCHER_DEBUG)           \
            logger_debug(__VA_ARGS__); \
    } while(0)

#endif /* __ERSWITCHER_PORTS_LOGGER__H__ */
