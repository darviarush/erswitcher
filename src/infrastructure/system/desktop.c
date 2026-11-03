#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "desktop.h"

extern char** environ;

int desktop_goto_home(void) {
    const char* home = getenv("HOME");
    if(!home) {
        fprintf(stderr, "WARN: нет getenv(HOME)\n");
        return 0;
    }
    if(chdir(home)) {
        fprintf(stderr, "WARN: не могу перейти в каталог пользователя %s\n", home);
        return 0;
    }
    setenv("PWD", home, 1);
    return 1;
}

void desktop_init(const char* program) {
    if(!program) return;

    const char* app = ".local/share/applications/erswitcher.desktop";

    if(access(app, R_OK) != 0) {
        mkdir(".local", 0700);
        mkdir(".local/share", 0700);
        mkdir(".local/share/applications", 0700);

        FILE* f = fopen(app, "wb");
        if(!f) {
            fprintf(stderr, "WARN: не могу создать %s/%s\n",
                    getenv("HOME") ? getenv("HOME") : "", app);
        } else {
            fprintf(f,
                    "[Desktop Entry]\n"
                    "Encoding=UTF-8\n"
                    "Terminal=false\n"
                    "Type=Application\n"
                    "Name=EN-RU Switcher\n"
                    "Comment=Transliterator for keyboard input\n"
                    "GenericName[ru]=Русско-английский клавиатурный переключатель\n"
                    "Comment[ru]=Транслитератор ввода с клавиатуры\n"
                    "Exec=%s\n"
                    "X-GNOME-Autostart-enabled=true\n"
                    "Icon=preferences-desktop-keyboard\n"
                    "Categories=Keyboard;Transliterate;Development;System;\n",
                    program);
            fclose(f);
        }
    }

    const char* autostart = ".config/autostart/erswitcher.desktop";

    if(access(autostart, R_OK) != 0) {
        mkdir(".config", 0700);
        mkdir(".config/autostart", 0700);

        FILE* f = fopen(autostart, "wb");
        if(!f) {
            fprintf(stderr, "WARN: не могу создать %s/%s\n",
                    getenv("HOME") ? getenv("HOME") : "", autostart);
            return;
        }

        fprintf(f,
                "[Desktop Entry]\n"
                "Name=EN-RU Switcher\n"
                "Comment=Transliterator for keyboard input\n"
                "Exec=%s\n"
                "Icon=preferences-desktop-keyboard\n"
                "Terminal=false\n"
                "Type=Application\n"
                "Categories=Keyboard;Transliterate;Development;System;\n"
                "Keywords=keyboard;transliterate;erswitcher;switch\n"
                "X-GNOME-UsesNotifications=t\n",
                program);
        fclose(f);
    }
}

void desktop_check_any_instance(void) {
    // TODO: килять другой экземпляр процесса, если он есть программно
}

pid_t desktop_launch_configurator(pid_t previous_pid) {
    if(previous_pid) {
        kill(previous_pid, SIGKILL);
    }

    pid_t pid = fork();
    if(pid < 0) {
        perror("ERROR: fork не запустился");
        return 0;
    }

    if(pid == 0) {
        const char* path = ".local/bin/erswitcher-configurator.tcl";
        char* const argv[] = {NULL};
        execve(path, argv, environ);
        fprintf(stderr, "ERROR: конфигуратор %s не запущен! Ошибка: %s\n",
                path, strerror(errno));
        exit(1);
    }

    return pid;
}
