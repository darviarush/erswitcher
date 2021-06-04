#ifndef __ERSWITCHER__H__
#define __ERSWITCHER__H__

/**
 * Основные функции иксов
 */

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <wchar.h>
#include <stdio.h>

// интервал проверки клавиатуры
#define DELAY	10000

extern Display* d;
extern int xerror;

// возвращает текущее окно
Window get_current_window();


#endif
