#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clipboard.h"
#include "../../ports/logger.h"

clipboard_x11_t er_clipboard;

static clipboard_x11_t* state_of(clipboard_port_t* port) {
    return (clipboard_x11_t*) port->context;
}

static Atom selection_atom(clipboard_x11_t* state, clipboard_selection_t selection) {
    switch(selection) {
    case CLIPBOARD_SELECTION_CLIPBOARD:
        return state->clipboard_atom;
    }
    return None;
}

static Atom format_atom(clipboard_x11_t* state, clipboard_format_t format) {
    switch(format) {
    case CLIPBOARD_FORMAT_UTF8:
        return state->utf8_atom;
    }
    return None;
}

static clipboard_format_t atom_format(clipboard_x11_t* state, Atom atom) {
    if(atom == state->utf8_atom) return CLIPBOARD_FORMAT_UTF8;
    return CLIPBOARD_FORMAT_UTF8;
}

static void clear_send_data(clipboard_x11_t* state) {
    if(state->send_data) {
        free(state->send_data);
        state->send_data = NULL;
    }
    state->send_pos = NULL;
    state->send_len = 0;
}

static void clear_receive_data(clipboard_x11_t* state) {
    if(state->receive_data) {
        free(state->receive_data);
        state->receive_data = NULL;
    }
    state->receive_len = 0;
    state->receive_type = None;
}

// Данные принадлежат callback только на время вызова; затем освобождаются
static void deliver(clipboard_x11_t* state, char* data, int len, Atom type) {
    clipboard_get_callback_t callback = state->pending_callback;
    void* context = state->pending_context;
    state->pending_callback = NULL;
    state->pending_context = NULL;

    if(callback) callback(context, data ? data : "", len, atom_format(state, type));

    free(data);
}

static void get_impl(clipboard_x11_t* state, clipboard_selection_t selection,
                     clipboard_format_t format, clipboard_get_callback_t callback,
                     void* callback_context) {
    Atom sel = selection_atom(state, selection);
    Atom fmt = format_atom(state, format);

    state->pending_callback = callback;
    state->pending_context = callback_context;

    if(!state->display) {
        deliver(state, strdup(""), 0, fmt);
        return;
    }

    Window owner = XGetSelectionOwner(state->display, sel);
    if(owner == None) {
        char* name = XGetAtomName(state->display, sel);
        fprintf(stderr, "Буфер %s не поддерживается владельцем\n", name ? name : "(none)");
        if(name) XFree(name);

        deliver(state, strdup(""), 0, state->utf8_atom);
        return;
    }

    DEBUG_LOG("get_selection: owner = 0x%lX %s\n", owner,
              state->window == owner ? "одинаков с w" : "разный с w");

    // Если владелец буфера мы же, отдаём копию своих данных сразу
    if(state->window == owner) {
        DEBUG_LOG("get_selection: буфер - мы же\n");

        if(state->send_data) {
            char* copy = malloc((size_t) state->send_len + 1);
            if(!copy)
                copy = strdup("");
            else {
                memcpy(copy, state->send_data, (size_t) state->send_len);
                copy[state->send_len] = '\0';
            }
            deliver(state, copy, state->send_len, state->send_target);
        } else {
            deliver(state, strdup(""), 0, state->utf8_atom);
        }
        return;
    }

    XConvertSelection(state->display, sel, fmt, state->sel_data_atom,
                      state->window, CurrentTime);
    XSync(state->display, False);

    if(ERSWITCHER_DEBUG) {
        char* fmt_name = XGetAtomName(state->display, fmt);
        DEBUG_LOG("get_selection %s!\n", fmt_name ? fmt_name : "(none)");
        if(fmt_name) XFree(fmt_name);
    }
}

static void set_impl(clipboard_x11_t* state, char* data, int len,
                     clipboard_format_t format) {
    clear_send_data(state);

    state->send_data = data;
    state->send_len = len;
    state->send_target = format_atom(state, format);

    if(state->send_len > state->selection_chunk_size)
        fprintf(stderr, "WARN: Размер данных для буфера обмена превышает размер куска для отправки: %d > %d. Передача будет осуществляться по протоколу INCR\n",
                state->send_len, state->selection_chunk_size);

    XSetSelectionOwner(state->display, state->clipboard_atom, state->window, CurrentTime);
    XFlush(state->display);

    Window owner = XGetSelectionOwner(state->display, state->clipboard_atom);
    DEBUG_LOG("set_clipboard `%s`, my window %s owner\n", data ? data : "",
              state->window == owner ? "is" : "no");
}

static void clear_impl(clipboard_x11_t* state) {
    clear_send_data(state);
}

static void selection_notify(clipboard_x11_t* state, XEvent* event) {
    if(event->xselection.selection != state->clipboard_atom) {
        fprintf(stderr, "SelectionNotify: Какой-то другой буфер запрошен\n");
        return;
    }

    if(event->xselection.property == None) {
        fprintf(stderr, "SelectionNotify: Нет выделения\n");
        state->incr_is = False;
        clear_receive_data(state);
        deliver(state, strdup(""), 0, state->utf8_atom);
        return;
    }

    Window requestor = event->xselection.requestor;
    Atom property = event->xselection.property;

    if(state->incr_is) {
        if(state->incr_window != requestor) {
            fprintf(stderr, "SelectionNotify: Не совпал incr_window\n");
            return;
        }
        if(state->incr_property != property) {
            fprintf(stderr, "SelectionNotify: Не совпал incr_propid\n");
            return;
        }
    } else {
        state->incr_property = property;
        state->incr_window = requestor;
    }

    int format = 0;
    unsigned long bytesafter = 0;
    unsigned long length = 0;
    unsigned char* result = NULL;

    XGetWindowProperty(event->xselection.display,
                       state->incr_window,
                       state->incr_property,
                       0L,
                       1024 * 1024 * 64,
                       state->incr_is, (Atom) AnyPropertyType,
                       &state->receive_type,
                       &format, &length, &bytesafter,
                       &result);

    if(ERSWITCHER_DEBUG) {
        char* type_name = XGetAtomName(state->display, state->receive_type);
        DEBUG_LOG("read selection: s=`%s` len=%lu after=%lu selection_retrive_type=%s format=%i\n",
                  result ? (char*) result : "", length, bytesafter,
                  type_name ? type_name : "(none)", format);
        if(type_name) XFree(type_name);
    }

    if(state->incr_is) {
        state->receive_data = realloc(state->receive_data,
                                      (size_t) state->receive_len + length + 1);
        if(state->receive_data) {
            if(result && length) memcpy(state->receive_data + state->receive_len, result, length);
            state->receive_len += (int) length;
            state->receive_data[state->receive_len] = '\0';
        }

        if(bytesafter == 0) {
            state->incr_is = False;

            char* data = state->receive_data;
            int len = state->receive_len;
            Atom type = state->receive_type;
            state->receive_data = NULL;
            state->receive_len = 0;
            deliver(state, data, len, type);
        }
    } else {
        Atom type = state->receive_type;

        clear_receive_data(state);
        state->receive_type = type;

        state->receive_data = malloc(length + bytesafter + 1);
        if(state->receive_data) {
            if(result && length) memcpy(state->receive_data, result, length);
            state->receive_len = (int) length;
            state->receive_data[state->receive_len] = '\0';
        }

        if(type == state->incr_atom) {
            state->incr_is = True;
        } else {
            char* data = state->receive_data;
            int len = state->receive_len;
            state->receive_data = NULL;
            state->receive_len = 0;
            deliver(state, data, len, type);
        }
    }

    if(result) XFree(result);
}

static void selection_request(clipboard_x11_t* state, XEvent* event) {
    Window win = event->xselectionrequest.requestor;
    Atom pty = event->xselectionrequest.property;
    Atom target_sel = event->xselectionrequest.target;

    if(target_sel == state->targets_atom) {
        Atom types[] = {state->send_target, state->targets_atom};

        XChangeProperty(state->display, win, pty, XA_ATOM, 32, PropModeReplace,
                        (unsigned char*) types, sizeof(types) / sizeof(Atom));

        DEBUG_LOG("отправляем targets\n");
    } else if(target_sel != state->send_target || pty == None) {
        if(ERSWITCHER_DEBUG) {
            char* name = XGetAtomName(state->display, target_sel);
            DEBUG_LOG("Запрошен clipboard в формате '%s'\n",
                      name ? name : "(none)");
            if(name) XFree(name);
        }
        return;
    } else if(state->send_len > state->selection_chunk_size) {
        XChangeProperty(state->display, win, pty, state->incr_atom, 32,
                        PropModeReplace, 0, 0);
        XSelectInput(state->display, win, PropertyChangeMask);

        state->send_pos = state->send_data;
    } else {
        XChangeProperty(state->display, win, pty, target_sel, 8, PropModeReplace,
                        (unsigned char*) state->send_data, state->send_len);
        DEBUG_LOG("отправляем clipboard_s: `%s` len=%i\n",
                  state->send_data ? state->send_data : "", state->send_len);
    }

    XEvent res;
    memset(&res, 0, sizeof res);
    res.xselection.type = SelectionNotify;
    res.xselection.display = event->xselectionrequest.display;
    res.xselection.requestor = win;
    res.xselection.property = pty;
    res.xselection.selection = event->xselectionrequest.selection;
    res.xselection.target = target_sel;
    res.xselection.time = event->xselectionrequest.time;

    XSendEvent(state->display, win, 0, 0, &res);
    XFlush(state->display);
}

static void property_notify(clipboard_x11_t* state, XEvent* event) {
    if(event->xproperty.state != PropertyDelete) return;
    if(!state->send_data) return;
    if(state->send_len <= state->selection_chunk_size) return;

    DEBUG_LOG("state == PropertyDelete\n");

    int len = state->send_pos + state->selection_chunk_size < state->send_data + state->send_len ? state->selection_chunk_size : (int) (state->send_data + state->send_len - state->send_pos);

    XChangeProperty(state->display,
                    event->xproperty.window,
                    event->xproperty.atom,
                    state->send_target,
                    8, PropModeReplace,
                    len == 0 ? NULL : (unsigned char*) state->send_pos,
                    len);
    XFlush(state->display);

    state->send_pos += len;
}

int clipboard_x11_handle_event(clipboard_x11_t* state, XEvent* event) {
    switch(event->type) {
    case SelectionNotify:
        selection_notify(state, event);
        return 1;
    case SelectionClear:
        fprintf(stderr, "SelectionClear: Утрачено право собственности на буфер обмена.\n");
        clear_send_data(state);
        return 1;
    case SelectionRequest:
        selection_request(state, event);
        return 1;
    case PropertyNotify:
        property_notify(state, event);
        return 1;
    default:
        return 0;
    }
}

void clipboard_x11_init(clipboard_x11_t* state, Display* display, Window window) {
    state->display = display;
    state->window = window;

    state->sel_data_atom = XInternAtom(display, "XSEL_DATA", False);
    state->utf8_atom = XInternAtom(display, "UTF8_STRING", True);
    if(state->utf8_atom == None) state->utf8_atom = XA_STRING;
    state->clipboard_atom = XInternAtom(display, "CLIPBOARD", False);
    state->targets_atom = XInternAtom(display, "TARGETS", False);
    state->incr_atom = XInternAtom(display, "INCR", False);

    state->selection_chunk_size = XExtendedMaxRequestSize(display) / 4;
    if(!state->selection_chunk_size) state->selection_chunk_size = XMaxRequestSize(display) / 4;

    DEBUG_LOG("selection_chunk_size = %i\n", state->selection_chunk_size);
}

void clipboard_x11_reset(clipboard_x11_t* state) {
    clear_send_data(state);
    clear_receive_data(state);
    state->pending_callback = NULL;
    state->pending_context = NULL;
    state->incr_is = False;
    state->incr_property = None;
    state->incr_window = None;
}

static void cb_get(clipboard_port_t* port, clipboard_selection_t selection,
                   clipboard_format_t format, clipboard_get_callback_t callback,
                   void* callback_context) {
    get_impl(state_of(port), selection, format, callback, callback_context);
}

static void cb_set(clipboard_port_t* port, char* data, int len,
                   clipboard_format_t format) {
    set_impl(state_of(port), data, len, format);
}

static void cb_clear(clipboard_port_t* port) {
    clear_impl(state_of(port));
}

void clipboard_x11_port(clipboard_port_t* port, clipboard_x11_t* state) {
    memset(port, 0, sizeof(*port));
    port->context = state;
    port->get = cb_get;
    port->set = cb_set;
    port->clear = cb_clear;
}
