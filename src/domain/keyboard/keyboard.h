#ifndef __ERSWITCHER_DOMAIN_KEYBOARD__H__
#define __ERSWITCHER_DOMAIN_KEYBOARD__H__

#include <stdlib.h>

// TODO: задержку указывать в конфиге
// дефолтная задержка
#define ERSWITCHER_DOMAIN_KEYBOARD_DELAY 		    10000
// .0 - обязателен, иначе деление будет целочисленным
#define USEC_TO_DBL(U)	(U / 1000000.0)


typedef struct {
    int delay = ERSWITCHER_DOMAIN_KEYBOARD_DELAY;      // задержка между программными нажатиями клавишь в микросекундах
    int loop_delay = ERSWITCHER_DOMAIN_KEYBOARD_DELAY; // задержка между опросами клавиатуры в микросекундах
    pid_t config_window_pid = 0;	// pid конфигуратора
} keyboard_t;

#endif /* __ERSWITCHER_DOMAIN_KEYBOARD__H__ */
