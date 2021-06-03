#ifndef __KEYBOARD__H__
#define __KEYBOARD__H__

/**
 * Клавиатура
 *
 * Заполняет массивы для сопоставления сканкодов и wchar_t
 */

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <wchar.h>

// сопоставление символа юникода с клавишей
typedef struct {
	wchar_t key;	// символ юникода
	int group;		// номер клавиатуры
	int code;		// скан-код клавиатуры
	int mods;		// модификаторы, которые должны быть нажаты
} unikey_t;

// интервал проверки клавиатуры
#define DELAY	10000

extern Display* d;
extern int xerror;

extern int groups;			// Количество раскладок
extern int group_ru;		// Номер русской раскладки или -1
extern int group_en;		// Номер английской раскладки или -1


// инициализирует названия клавиатуры
// вызывается из keysym_init()
void init_keyboard();

/** Создаёт массив с символами юникода, которые можно ввести 
 * с задействованных в системе раскладок клавиатуры.
 * Массив отсортирован в порядке возрастания символов юникода,
 * чтобы можно было быстро найти его методом дихотомии.
 * Длина массива записывается в keysyms.
 */
extern unikey_t keysym[];
extern int keysyms;

// инициализирует массив и его длину
void keysym_init();

// возвращает указатель на символ по коду или NULL
unikey_t* get_key(wchar_t cs);

// Переключает клавиатуру
void set_group(int group);

// возвращает текущую раскладку
int get_group();

// возвращает модификаторы
int get_mods();

// устанавливает модификаторы
void set_mods(int mods);

// эмулирует ввод текста
void type(wchar_t* s);

// Эмулирует нажатие и отжатие клавиши
void press_key(wchar_t cs);

// Эмулирует нажатие или отжатие клавиши
void send_key(wchar_t cs, int is_press);

#endif
