#ifndef __COPYPASTE__H__
#define __COPYPASTE__H__

/**
 * Копирование текста
 */

#include <X11/Xatom.h>

// копируем в наш буфер выделение
char* copy_selection(Atom number_buf);
// в буфер
int to_buffer(char** s);

#endif
