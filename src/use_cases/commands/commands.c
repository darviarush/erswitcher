#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands.h"

static void commands_use_case_finish(commands_use_case_t* self) {
    free(self->command);
    self->command = NULL;
    self->capture_output = 0;
    self->waiting_clipboard = 0;
    self->running = 0;
    self->progress = 0;
}

static void commands_use_case_insert(commands_use_case_t* self, const char* text) {
    if(!self->insert_text || !text) return;
    self->insert_text(self->insert_context, text);
}

static void commands_use_case_tick(void* context);

static void commands_use_case_start(commands_use_case_t* self,
                                    const char* input, int input_len) {
    char* prepared = self->runner->prepare(self->runner, self->command,
                                           input, input_len);
    if(!prepared) {
        commands_use_case_finish(self);
        return;
    }

    int started = self->runner->start(self->runner, prepared,
                                      self->capture_output);
    if(started != 1) {
        commands_use_case_finish(self);
        return;
    }

    self->waiting_clipboard = 0;
    self->running = 1;
    self->progress = 0;

    self->scheduler->after(self->scheduler, self->interval,
                           commands_use_case_tick, self);
}

static void commands_use_case_tick(void* context) {
    commands_use_case_t* self = (commands_use_case_t*) context;

    if(!self->running || !self->command) return;

    command_runner_status_t status = self->runner->poll(self->runner);
    if(status == COMMAND_RUNNER_RUNNING) {
        self->progress += self->interval;
        if(self->progress >= self->timeout) {
            self->runner->cancel(self->runner);

            char message[64];
            snprintf(message, sizeof message, "TIMEOUT %gs!", self->timeout);
            commands_use_case_insert(self, message);
            commands_use_case_finish(self);
            return;
        }

        self->scheduler->after(self->scheduler, self->interval,
                               commands_use_case_tick, self);
        return;
    }

    if(status == COMMAND_RUNNER_DONE && self->capture_output) {
        int len = 0;
        const char* output = self->runner->output(self->runner, &len);
        if(output && len > 0) commands_use_case_insert(self, output);
    }

    commands_use_case_finish(self);
}

static void commands_use_case_clipboard_cb(void* context, const char* data,
                                           int len, clipboard_format_t format) {
    (void) format;
    commands_use_case_t* self = (commands_use_case_t*) context;

    if(!self->waiting_clipboard || !self->command) return;
    commands_use_case_start(self, data ? data : "", data ? len : 0);
}

void commands_use_case_init(commands_use_case_t* self,
                            command_runner_port_t* runner,
                            clipboard_port_t* clipboard,
                            scheduler_port_t* scheduler,
                            void (*request_selection)(void* context),
                            void* request_context,
                            void (*insert_text)(void* context, const char* text),
                            void* insert_context,
                            double timeout, double interval) {
    self->runner = runner;
    self->clipboard = clipboard;
    self->scheduler = scheduler;
    self->request_selection = request_selection;
    self->request_context = request_context;
    self->insert_text = insert_text;
    self->insert_context = insert_context;
    self->timeout = timeout;
    self->interval = interval;
    self->command = NULL;
    self->capture_output = 0;
    self->waiting_clipboard = 0;
    self->running = 0;
    self->progress = 0;
}

void commands_use_case_set_options(commands_use_case_t* self,
                                   double timeout, double interval) {
    if(!self) return;
    self->timeout = timeout;
    self->interval = interval;
}

void commands_use_case_run(commands_use_case_t* self, const char* command) {
    if(!self || !self->runner || !self->scheduler) return;
    if(!command || commands_use_case_is_blank(command)) return;
    if(self->command) return;

    self->command = strdup(command);
    if(!self->command) return;

    self->capture_output = strstr(self->command, "%o") != NULL;

    if(strstr(self->command, "%i")) {
        self->waiting_clipboard = 1;

        if(self->request_selection) {
            self->request_selection(self->request_context);
        }

        if(!self->clipboard) {
            commands_use_case_finish(self);
            return;
        }

        self->clipboard->get(self->clipboard,
                             CLIPBOARD_SELECTION_CLIPBOARD,
                             CLIPBOARD_FORMAT_UTF8,
                             commands_use_case_clipboard_cb, self);
        return;
    }

    commands_use_case_start(self, "", 0);
}

void commands_use_case_reset(commands_use_case_t* self) {
    if(!self) return;

    if(self->running && self->runner && self->runner->cancel) {
        self->runner->cancel(self->runner);
    }

    commands_use_case_finish(self);
}

int commands_use_case_busy(const commands_use_case_t* self) {
    return self && self->command != NULL;
}

int commands_use_case_is_blank(const char* command) {
    if(!command) return 1;

    const unsigned char* s = (const unsigned char*) command;
    while(isspace(*s))
        s++;
    return *s == '\0';
}
