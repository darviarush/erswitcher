#ifndef __ERSWITCHER_USE_CASES_COMMANDS__H__
#define __ERSWITCHER_USE_CASES_COMMANDS__H__

#include "../../ports/clipboard.h"
#include "../../ports/command_runner.h"
#include "../../ports/scheduler.h"

/*
 * Сценарий [commands]
 *
 * Он не делает fork, не пишет временные файлы и не читает часы:
 *  - command_runner готовит/запускает процесс и отдаёт вывод %o;
 *  - clipboard используется для %i (сначала Ctrl+Insert, затем get);
 *  - scheduler отсчитывает интервал опроса и таймаут;
 *  - insert_text вставляет результат (в приложении — через snippets)
 *
 * Состояние одного активного выполнения хранится здесь, поэтому
 * параллельный запуск команд запрещён, как и в старом монолите
 */
typedef struct {
    command_runner_port_t* runner;
    clipboard_port_t* clipboard;
    scheduler_port_t* scheduler;

    void (*request_selection)(void* context);
    void* request_context;

    void (*insert_text)(void* context, const char* text);
    void* insert_context;

    double timeout;
    double interval;

    char* command;
    int capture_output;
    int waiting_clipboard;
    int running;
    double progress;
} commands_use_case_t;

void commands_use_case_init(commands_use_case_t* self,
                            command_runner_port_t* runner,
                            clipboard_port_t* clipboard,
                            scheduler_port_t* scheduler,
                            void (*request_selection)(void* context),
                            void* request_context,
                            void (*insert_text)(void* context, const char* text),
                            void* insert_context,
                            double timeout, double interval);

void commands_use_case_set_options(commands_use_case_t* self,
                                   double timeout, double interval);

void commands_use_case_run(commands_use_case_t* self, const char* command);
void commands_use_case_reset(commands_use_case_t* self);

int commands_use_case_busy(const commands_use_case_t* self);
int commands_use_case_is_blank(const char* command);

#endif /* __ERSWITCHER_USE_CASES_COMMANDS__H__ */
