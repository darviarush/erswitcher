#include <sys/stat.h>
#include <sys/wait.h>

#include "test.h"

/*
 * Системная интеграция: desktop/autostart-файлы и переход в $HOME
 * Запуск конфигуратора здесь не проверяем: это fork/exec, его покрывает
 * ручной smoke с реальным треем
 */

START_TEST(er_desktop_creates_files) {
    desktop_init("/opt/erswitcher");

    char path[PATH_MAX];
    char buf[4096];

    er_path(path, sizeof path, ".local/share/applications/erswitcher.desktop");
    ck_assert_ptr_nonnull(strstr(er_read(path, buf, sizeof buf), "Exec=/opt/erswitcher"));
    ck_assert_ptr_nonnull(strstr(buf, "X-GNOME-Autostart-enabled=true"));
    ck_assert_ptr_nonnull(strstr(buf, "Name=EN-RU Switcher"));

    er_path(path, sizeof path, ".config/autostart/erswitcher.desktop");
    ck_assert_ptr_nonnull(strstr(er_read(path, buf, sizeof buf), "Exec=/opt/erswitcher"));
    ck_assert_ptr_nonnull(strstr(buf, "X-GNOME-UsesNotifications=t"));
}
END_TEST

START_TEST(er_desktop_does_not_overwrite_existing) {
    char path[PATH_MAX];

    desktop_init("/opt/erswitcher");

    er_path(path, sizeof path, ".local/share/applications/erswitcher.desktop");
    ck_assert(er_write(path, "keep-app"));

    er_path(path, sizeof path, ".config/autostart/erswitcher.desktop");
    ck_assert(er_write(path, "keep-autostart"));

    desktop_init("/opt/erswitcher");

    char buf[64];
    er_path(path, sizeof path, ".local/share/applications/erswitcher.desktop");
    ck_assert_str_eq(er_read(path, buf, sizeof buf), "keep-app");

    er_path(path, sizeof path, ".config/autostart/erswitcher.desktop");
    ck_assert_str_eq(er_read(path, buf, sizeof buf), "keep-autostart");
}
END_TEST

START_TEST(er_desktop_launches_configurator_from_appdir) {
    char appdir[PATH_MAX];
    char bindir[PATH_MAX];
    char script[PATH_MAX];
    char marker[PATH_MAX];

    snprintf(appdir, sizeof appdir, "%s/appdir", er_env.home);
    ck_assert_int_eq(mkdir(appdir, 0700), 0);
    snprintf(bindir, sizeof bindir, "%s/usr", appdir);
    ck_assert_int_eq(mkdir(bindir, 0700), 0);
    snprintf(bindir, sizeof bindir, "%s/usr/bin", appdir);
    ck_assert_int_eq(mkdir(bindir, 0700), 0);

    snprintf(script, sizeof script, "%s/usr/bin/erswitcher-configurator.tcl",
             appdir);
    ck_assert(er_write(script, "#!/bin/sh\ntouch \"$HOME/launched-appdir\"\n"));
    ck_assert_int_eq(chmod(script, 0755), 0);

    ck_assert_int_eq(setenv("APPDIR", appdir, 1), 0);
    pid_t pid = desktop_launch_configurator(0);
    ck_assert_int_gt(pid, 0);

    int status = 0;
    ck_assert_int_eq(waitpid(pid, &status, 0), pid);
    ck_assert_int_eq(WIFEXITED(status), 1);
    ck_assert_int_eq(WEXITSTATUS(status), 0);
    ck_assert_int_eq(unsetenv("APPDIR"), 0);

    er_path(marker, sizeof marker, "launched-appdir");
    ck_assert_int_eq(access(marker, F_OK), 0);
}
END_TEST

START_TEST(er_desktop_goto_home) {
    char sub[PATH_MAX];
    char cwd[PATH_MAX];

    snprintf(sub, sizeof sub, "%s", er_env.home);
    strncat(sub, "/sub", sizeof sub - strlen(sub) - 1);
    ck_assert_int_eq(mkdir(sub, 0700), 0);
    ck_assert_int_eq(chdir(sub), 0);

    ck_assert_int_eq(desktop_goto_home(), 1);
    ck_assert_ptr_nonnull(getcwd(cwd, sizeof cwd));
    ck_assert_str_eq(cwd, er_env.home);
    ck_assert_str_eq(getenv("PWD"), er_env.home);
}
END_TEST

START_TEST(er_desktop_launches_configurator) {
    char path[PATH_MAX];
    char marker[PATH_MAX];

    ck_assert_int_eq(mkdir(".local", 0700), 0);
    ck_assert_int_eq(mkdir(".local/bin", 0700), 0);

    er_path(path, sizeof path, ".local/bin/erswitcher-configurator.tcl");
    ck_assert(er_write(path, "#!/bin/sh\ntouch \"$HOME/launched\"\n"));
    ck_assert_int_eq(chmod(path, 0755), 0);

    pid_t pid = desktop_launch_configurator(0);
    ck_assert_int_gt(pid, 0);

    int status = 0;
    ck_assert_int_eq(waitpid(pid, &status, 0), pid);
    ck_assert_int_eq(WIFEXITED(status), 1);
    ck_assert_int_eq(WEXITSTATUS(status), 0);

    er_path(marker, sizeof marker, "launched");
    ck_assert_int_eq(access(marker, F_OK), 0);
}
END_TEST

static TCase* tcase_desktop() {
    TCase* tc = tcase_create("desktop");
    tcase_add_checked_fixture(tc, er_fixture, er_fixture_teardown);
    tcase_add_test(tc, er_desktop_creates_files);
    tcase_add_test(tc, er_desktop_does_not_overwrite_existing);
    tcase_add_test(tc, er_desktop_goto_home);
    tcase_add_test(tc, er_desktop_launches_configurator);
    tcase_add_test(tc, er_desktop_launches_configurator_from_appdir);
    return tc;
}

Suite* suite_desktop() {
    Suite* s = suite_create("desktop");
    suite_add_tcase(s, tcase_desktop());
    return s;
}
