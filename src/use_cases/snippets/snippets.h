#ifndef __ERSWITCHER_USE_CASES_SNIPPETS__H__
#define __ERSWITCHER_USE_CASES_SNIPPETS__H__

#include "../../ports/clipboard.h"
#include "../../ports/keyboard.h"
#include "../../ports/scheduler.h"

/*
 * Вставка сниппета через буфер обмена
 *
 * Текст копируется в clipboard-порт, затем через scheduler ставится
 * отложенное Shift+Insert. Сами нажатия выполняются через keyboard-порт,
 * поэтому сценарий не зависит от X11
 */
typedef struct {
    clipboard_port_t* clipboard;
    scheduler_port_t* scheduler;
    keyboard_port_t* keyboard;
} snippets_use_case_t;

void snippets_use_case_init(snippets_use_case_t* self,
                            clipboard_port_t* clipboard,
                            scheduler_port_t* scheduler,
                            keyboard_port_t* keyboard);

void snippets_use_case_insert(snippets_use_case_t* self, const char* text);

#endif /* __ERSWITCHER_USE_CASES_SNIPPETS__H__ */
