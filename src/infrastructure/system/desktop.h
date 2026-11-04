#ifndef __ERSWITCHER_INFRASTRUCTURE_SYSTEM_DESKTOP__H__
#define __ERSWITCHER_INFRASTRUCTURE_SYSTEM_DESKTOP__H__

#include <sys/types.h>

/*
 * Системная интеграция: переход в $HOME, desktop/autostart-файлы и
 * запуск конфигуратора. Все пути относительны $HOME, как в старом
 * монолите.
 */

int desktop_goto_home(void);

void desktop_init(const char* program);

void desktop_check_any_instance(void);

/* Убивает предыдущий конфигуратор (если pid != 0), запускает новый и
 * возвращает его pid; 0 — если fork не удался */
pid_t desktop_launch_configurator(pid_t previous_pid);

#endif /* __ERSWITCHER_INFRASTRUCTURE_SYSTEM_DESKTOP__H__ */
