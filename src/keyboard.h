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


// количество клавишь клавиатуры
#define KEYBOARD_SIZE	(32*8)

// Установлен ли бит
#define BIT(VECTOR, BIT_IDX)   ( ((char*)VECTOR)[BIT_IDX/8]&(1<<(BIT_IDX%8)) )

// дефолтная задержка
#define DELAY 		10000

extern int delay;			// задержка после нажатия клавиши

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
//void set_mods(int mods);

// эмулирует ввод текста
void type(char* s);

// Эмулирует нажатие и отжатие клавиши
void press_key(KeySym ks);

// Эмулирует нажатие или отжатие клавиши
void send_key(KeySym ks, int is_press);

// сопоставляет символ из другой раскладки
KeySym translate(KeySym ks);

// "нажимает" lock, если он нажат
void reset_mods(int mods);

// устанавливает модификаторы путём ввода клавиш
void send_mods(int mods, int is_press);

// возвращает модификатры с кнопками мыши
unsigned int get_input_state();

// активные модификаторы
int get_active_mods(int *keys);
void clear_active_mods(int* keys, int nkeys);
void set_active_mods(int* keys, int nkeys);

#endif
