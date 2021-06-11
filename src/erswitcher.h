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

// размер буферов trans и word
#define BUF_SIZE	1024

extern Display* d;
extern int xerror;
extern wint_t *word, *trans;
extern int pos;

// меняет буфера местами
inline void swap_buf() {
	wint_t* swap = word; word = trans; trans = swap;
}

// возвращает текущее окно
Window get_current_window();

// подключается к дисплею
void open_display();

// переподключается к дисплею
void reset_display();


#endif
