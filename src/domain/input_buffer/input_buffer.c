#include <string.h>
#include <wctype.h>

#include "input_buffer.h"

void input_buffer_init(input_buffer_t* buffer) {
    memset(buffer, 0, sizeof(*buffer));
}

void input_buffer_reset(input_buffer_t* buffer) {
    buffer->pos = 0;
}

void input_buffer_push(input_buffer_t* buffer, unikey_t key, wint_t symbol) {
    // Поведение из монолита: при переполнении начинаем сначала
    if(buffer->pos >= INPUT_BUFFER_CAPACITY) buffer->pos = 0;

    buffer->word[buffer->pos] = key;
    buffer->symbol[buffer->pos] = symbol;
    buffer->pos++;
}

void input_buffer_backspace(input_buffer_t* buffer) {
    if(buffer->pos != 0) --buffer->pos;
}

int input_buffer_from_space(const input_buffer_t* buffer) {
    int last = buffer->pos - 1;

    // Пробелы в конце ввода остаются частью последнего слова: после
    // «слово » word translate должен перевести «слово », а не ничего.
    // Поэтому границу слова ищем до хвостовых пробелов, но возвращаем
    // начало слова — хвостовые пробелы попадут в диапазон [from, pos).
    while(last >= 0 && iswspace(buffer->symbol[last])) --last;

    // Начало слова — сразу после предыдущего пробела.
    while(last >= 0 && !iswspace(buffer->symbol[last])) --last;

    return last + 1;
}
