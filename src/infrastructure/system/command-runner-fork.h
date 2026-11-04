#ifndef __ERSWITCHER_INFRASTRUCTURE_SYSTEM_COMMAND_RUNNER_FORK__H__
#define __ERSWITCHER_INFRASTRUCTURE_SYSTEM_COMMAND_RUNNER_FORK__H__

#include <sys/types.h>

#include "../../ports/command-runner.h"

/*
 * fork/system-реализация command_runner
 *
 * Состояние содержит pid, временные пути и последний вывод %o. Один
 * экземпляр на процесс: параллельный запуск запрещён контрактом
 */
typedef struct {
    const char* infile;
    const char* outfile;

    pid_t pid;
    char* output;
    int output_len;
    int capture_output;
} command_runner_fork_t;

void command_runner_fork_port(command_runner_port_t* port,
                              command_runner_fork_t* state);

/* Сбрасывает состояние, не меняя пути infile/outfile */
void command_runner_fork_reset(command_runner_fork_t* state);

#endif /* __ERSWITCHER_INFRASTRUCTURE_SYSTEM_COMMAND_RUNNER_FORK__H__ */
