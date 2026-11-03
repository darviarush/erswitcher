#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "command_runner_fork.h"
#include "../../domain/command_template/command_template.h"

#define COMMAND_RUNNER_INFILE "/tmp/erswitcher-input"
#define COMMAND_RUNNER_OUTFILE "/tmp/erswitcher-output"

static command_runner_fork_t* state_of(command_runner_port_t* port) {
    return (command_runner_fork_t*) port->context;
}

static char* prepare_impl(command_runner_port_t* port, const char* command,
                          const char* input, int input_len) {
    if(!command) return NULL;

    command_runner_fork_t* state = state_of(port);

    FILE* f = fopen(state->infile, "wb");
    if(!f) {
        fprintf(stderr, "Временный infile `%s` не открыт!\n", state->infile);
        return NULL;
    }

    if(input && input_len > 0) {
        fwrite(input, 1, (size_t) input_len, f);
    }
    fclose(f);

    int invalid = 0;
    char* expanded = command_template_expand(command, state->infile,
                                             state->outfile, &invalid);
    if(invalid) {
        fprintf(stderr, "Замена в формате не валидна! `%s`\n", command);
    }

    return expanded;
}

static void free_output(command_runner_fork_t* state) {
    free(state->output);
    state->output = NULL;
    state->output_len = 0;
}

static void read_output(command_runner_fork_t* state) {
    free_output(state);

    FILE* f = fopen(state->outfile, "rb");
    if(!f) {
        fprintf(stderr, "Файл команды не открыт\n");
        return;
    }

    if(fseek(f, 0L, SEEK_END) != 0) {
        fclose(f);
        return;
    }

    long size = ftell(f);
    if(size < 0) size = 0;
    rewind(f);

    char* data = malloc((size_t) size + 1);
    if(!data) {
        fclose(f);
        return;
    }

    size_t n = fread(data, 1, (size_t) size, f);
    fclose(f);

    data[n] = '\0';
    if(n && data[n - 1] == '\n') data[--n] = '\0';

    state->output = data;
    state->output_len = (int) n;
}

static int start_impl(command_runner_port_t* port, char* command,
                      int capture_output) {
    command_runner_fork_t* state = state_of(port);

    if(state->pid > 0) {
        free(command);
        return 0;
    }

    free_output(state);
    state->capture_output = capture_output;

    pid_t pid = fork();
    if(pid == 0) {
        setpgid(0, 0);
        int status = system(command);
        exit(status);
    }

    if(pid < 0) {
        fprintf(stderr, "ERROR: команда не форкнулась! Ошибка: %s\n",
                strerror(errno));
        free(command);
        return -1;
    }

    free(command);
    state->pid = pid;
    return 1;
}

static command_runner_status_t poll_impl(command_runner_port_t* port) {
    command_runner_fork_t* state = state_of(port);

    if(state->pid <= 0) return COMMAND_RUNNER_DONE;

    int status = 0;
    pid_t completed = waitpid(state->pid, &status, WNOHANG);
    if(completed == 0) return COMMAND_RUNNER_RUNNING;
    if(completed < 0 && errno == EINTR) return COMMAND_RUNNER_RUNNING;

    if(state->capture_output) read_output(state);

    unlink(state->outfile);
    unlink(state->infile);
    state->pid = 0;

    return COMMAND_RUNNER_DONE;
}

static void cancel_impl(command_runner_port_t* port) {
    command_runner_fork_t* state = state_of(port);

    if(state->pid > 0) {
        kill(-state->pid, SIGKILL);
        waitpid(state->pid, NULL, WNOHANG);
        state->pid = 0;
    }

    unlink(state->outfile);
    unlink(state->infile);
    free_output(state);
    state->capture_output = 0;
}

static const char* output_impl(command_runner_port_t* port, int* len) {
    command_runner_fork_t* state = state_of(port);
    if(len) *len = state->output_len;
    return state->output ? state->output : "";
}

void command_runner_fork_port(command_runner_port_t* port,
                              command_runner_fork_t* state) {
    memset(port, 0, sizeof(*port));
    port->context = state;

    if(!state->infile) state->infile = COMMAND_RUNNER_INFILE;
    if(!state->outfile) state->outfile = COMMAND_RUNNER_OUTFILE;

    port->prepare = prepare_impl;
    port->start = start_impl;
    port->poll = poll_impl;
    port->cancel = cancel_impl;
    port->output = output_impl;
}

void command_runner_fork_reset(command_runner_fork_t* state) {
    if(!state) return;

    if(state->pid > 0) {
        kill(-state->pid, SIGKILL);
        waitpid(state->pid, NULL, WNOHANG);
        state->pid = 0;
    }

    if(state->outfile) unlink(state->outfile);
    if(state->infile) unlink(state->infile);

    free_output(state);
    state->capture_output = 0;
}
