#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "command_template.h"

typedef struct {
    char* data;
    size_t len;
    size_t cap;
} string_buf_t;

static int append(string_buf_t* buf, const char* s, size_t n) {
    size_t need = buf->len + n + 1;
    if(need > buf->cap) {
        size_t cap = buf->cap ? buf->cap : 64;
        while(cap < need)
            cap *= 2;

        char* data = realloc(buf->data, cap);
        if(!data) return 0;

        buf->data = data;
        buf->cap = cap;
    }

    if(n) memcpy(buf->data + buf->len, s, n);
    buf->len += n;
    buf->data[buf->len] = '\0';
    return 1;
}

char* command_template_expand(const char* command, const char* infile, const char* outfile, int* invalid) {
    if(invalid) *invalid = 0;
    if(!command) return NULL;

    string_buf_t buf = {0};

    for(const char* s = command; *s;) {
        if(*s != '%') {
            if(!append(&buf, s, 1)) goto fail;
            s++;
            continue;
        }

        char next = s[1];
        if(next == 'i') {
            if(!append(&buf, infile, strlen(infile))) goto fail;
            s += 2;
        } else if(next == 'o') {
            if(!append(&buf, outfile, strlen(outfile))) goto fail;
            s += 2;
        } else if(next == '%') {
            if(!append(&buf, "%", 1)) goto fail;
            s += 2;
        } else if(next == '\0') {
            // Незавершённый % в конце — оставляем как есть
            if(!append(&buf, "%", 1)) goto fail;
            s++;
        } else {
            // Неизвестная подстановка сохраняется буквально: %z
            if(invalid) (*invalid)++;
            if(!append(&buf, "%", 1)) goto fail;
            if(!append(&buf, &next, 1)) goto fail;
            s += 2;
        }
    }

    if(!buf.data) {
        buf.data = malloc(1);
        if(!buf.data) return NULL;
        buf.data[0] = '\0';
    }

    return buf.data;

fail:
    free(buf.data);
    return NULL;
}
