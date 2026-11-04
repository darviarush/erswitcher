#ifndef __ERSWITCHER_USE_CASES_INPUT__H__
#define __ERSWITCHER_USE_CASES_INPUT__H__

#include "../../domain/input-buffer/input-buffer.h"
#include "../../ports/keyboard.h"

/*
 * Сценарий ввода
 *
 * Владелец буфера — domain/input-buffer: word[], symbol[], pos
 * Сценарий не знает, откуда берутся клавиши; классификацию
 * (Backspace/управляющая/печатная) даёт порт клавиатуры
 */
typedef struct {
    input_buffer_t* buffer;
    keyboard_port_t* keyboard;
} input_use_case_t;

void input_use_case_init(input_use_case_t* self, input_buffer_t* buffer,
                         keyboard_port_t* keyboard);
void input_use_case_reset(input_use_case_t* self);
void input_use_case_push(input_use_case_t* self, unikey_t key);
void input_use_case_backspace(input_use_case_t* self);
void input_use_case_handle_key(input_use_case_t* self, unikey_t key);
int input_use_case_from_space(const input_use_case_t* self);

#endif /* __ERSWITCHER_USE_CASES_INPUT__H__ */
