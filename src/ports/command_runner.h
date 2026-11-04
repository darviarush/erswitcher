#ifndef __ERSWITCHER_PORTS_COMMAND_RUNNER__H__
#define __ERSWITCHER_PORTS_COMMAND_RUNNER__H__

/*
 * Порт запуска шелл-команд.
 *
 * Сценарий [commands] не должен знать про fork/system/waitpid, временные
 * файлы и таймауты. Порт:
 *
 *  - prepare() пишет входные данные во временный файл и раскрывает
 *    %i/%o/%% в шаблоне команды;
 *  - start() запускает подготовленную команду в отдельном процессе и
 *    забирает строку в собственность;
 *  - poll() не блокирует и сообщает, закончился ли процесс;
 *  - output() отдаёт захваченный вывод %o до следующего start()/cancel().
 *
 * Таймаут остаётся в сценарии: он отсчитывается через ports/scheduler,
 * а не через wall-clock API внутри use_cases.
 */

typedef struct command_runner_port command_runner_port_t;

typedef enum {
    COMMAND_RUNNER_RUNNING = 0,
    COMMAND_RUNNER_DONE,
    COMMAND_RUNNER_ERROR,
} command_runner_status_t;

struct command_runner_port {
    void* context;

    /* Возвращает malloc-строку или NULL. input == NULL считается пустым */
    char* (*prepare)(command_runner_port_t* self, const char* command,
                     const char* input, int input_len);

    /* Забирает command (malloc) в собственность в любом случае.
     * capture_output — читать ли временный outfile после завершения.
     * 1 — запущено, 0 — уже выполняется, -1 — ошибка */
    int (*start)(command_runner_port_t* self, char* command, int capture_output);

    /* Неблокирующий опрос. На DONE/ERROR временные файлы уже убраны */
    command_runner_status_t (*poll)(command_runner_port_t* self);

    /* Убивает процесс (если он есть) и освобождает временное состояние */
    void (*cancel)(command_runner_port_t* self);

    /* Вывод последней завершённой команды; пустая строка, если вывода нет.
     * Указатель живёт до следующего start()/cancel() */
    const char* (*output)(command_runner_port_t* self, int* len);
};

#endif /* __ERSWITCHER_PORTS_COMMAND_RUNNER__H__ */
