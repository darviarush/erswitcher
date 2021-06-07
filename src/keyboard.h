#ifndef __KEYBOARD__H__
#define __KEYBOARD__H__

/**
 * Клавиатура
 *
 * Заполняет массивы для сопоставления сканкодов и wint_t
 */

#include <X11/keysym.h>
#include <X11/extensions/XKB.h>
#include <xkbcommon/xkbcommon.h>


extern int groups;			// Количество раскладок
extern int group_ru;		// Номер русской раскладки или -1
extern int group_en;		// Номер английской раскладки или -1


// инициализирует названия клавиатуры
// вызывается из keysym_init()
void init_keyboard();

// инициализирует клавиатуру
void keysym_init();

// Переключает клавиатуру
void set_group(int group);

// возвращает текущую раскладку
int get_group();

// возвращает модификаторы
int get_mods();

// устанавливает модификаторы
void set_mods(int mods);

// эмулирует ввод текста
void type(KeySym* s);

// Эмулирует нажатие и отжатие клавиши
void press_key(KeySym ks);

// Эмулирует нажатие или отжатие клавиши
void send_key(KeySym ks, int is_press);

// сопоставляет символ из другой раскладки
KeySym translate(KeySym ks);

// "нажимает" lock, если он нажат
void reset_mods(int mods);

#endif
