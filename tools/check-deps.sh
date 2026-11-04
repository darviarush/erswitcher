#!/bin/sh
# Проверяет программы и pkg-config-модули, нужные для сборки, тестов,
# разработки, упаковки и релизов. Ничего не устанавливает.
set -u

fail=0

section() {
    printf '\n== %s ==\n' "$1"
}

ok() {
    printf '  [ok]   %s\n' "$1"
}

missing_required() {
    printf '  [FAIL] %s\n' "$1"
    fail=$((fail + 1))
}

missing_optional() {
    printf '  [--]   %s\n' "$1"
}

have_cmd() {
    command -v "$1" >/dev/null 2>&1
}

check_cmd() {
    label=$1
    cmd=$2
    kind=${3:-required}

    if have_cmd "$cmd"; then
        ok "$label: $cmd ($(command -v "$cmd"))"
    elif [ "$kind" = required ]; then
        missing_required "$label: не найден $cmd"
    else
        missing_optional "$label: не найден $cmd (опционально)"
    fi
}

check_pkg() {
    label=$1
    module=$2
    kind=${3:-required}

    if ! have_cmd pkg-config; then
        return
    fi

    if pkg-config --exists "$module"; then
        ok "$label: pkg-config $module"
    elif [ "$kind" = required ]; then
        missing_required "$label: нет pkg-config-модуля $module"
    else
        missing_optional "$label: нет pkg-config-модуля $module (опционально)"
    fi
}

section "Сборка"
check_cmd "C-компилятор" gcc
check_cmd "GNU Make" make
check_cmd "pkg-config" pkg-config
check_pkg "X11" x11
check_pkg "Xkbcommon" xkbcommon
check_pkg "XTest" xtst

section "Тесты"
check_pkg "Check" check
check_pkg "libbacktrace" libbacktrace
check_cmd "Tcl" tclsh
check_cmd "Tk/wish" wish

if [ -n "${DISPLAY:-}" ]; then
    ok "xvfb-run: не требуется (DISPLAY=$DISPLAY)"
elif have_cmd xvfb-run; then
    ok "xvfb-run: $(command -v xvfb-run)"
else
    missing_required "xvfb-run: нужен для make test-functional без DISPLAY"
fi

section "Разработка (опционально)"
check_cmd "clang-format" clang-format optional
check_cmd "xdotool" xdotool optional
check_cmd "valgrind" valgrind optional
check_cmd "gdb" gdb optional
check_cmd "meld" meld optional
check_cmd "git" git optional

section "Упаковка и релизы (опционально)"
check_cmd "appimagetool" appimagetool optional
check_cmd "gh" gh optional
check_cmd "curl" curl optional
check_cmd "rsvg-convert" rsvg-convert optional
check_cmd "desktop-file-validate" desktop-file-validate optional
check_cmd "yay" yay optional

printf '\n'
if [ "$fail" -ne 0 ]; then
    printf 'Итог: не хватает %i обязательных зависимостей\n' "$fail"
    exit 1
fi

printf 'Итог: обязательные зависимости на месте\n'
