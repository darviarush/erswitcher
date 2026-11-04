#!/bin/sh
# Проверяет зависимости из манифеста Dependencies.
# Категория выбирается переменной CATEGORY; make передаёт её из C=...
set -u

ROOT=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
MANIFEST="$ROOT/Dependencies"
CATEGORY=${CATEGORY:-}
only=
last=
fail=0

case "$CATEGORY" in
    ""|all|Все|все)
        only=
        ;;
    Сборка|сборка|build)
        only=build
        ;;
    Тесты|тесты|test)
        only=test
        ;;
    Разработка|разработка|dev|development)
        only=dev
        ;;
    Упаковка|упаковка|packaging|release)
        only=packaging
        ;;
    *)
        printf 'FAIL: неизвестная категория "%s"\n' "$CATEGORY" >&2
        printf 'Доступно: Сборка, Тесты, Разработка, Упаковка\n' >&2
        exit 2
        ;;
esac

if [ ! -r "$MANIFEST" ]; then
    printf 'FAIL: не найден манифест %s\n' "$MANIFEST" >&2
    exit 2
fi

section_title() {
    case "$1" in
        build) printf 'Сборка' ;;
        test) printf 'Тесты' ;;
        dev) printf 'Разработка' ;;
        packaging) printf 'Упаковка и релизы' ;;
        *) printf '%s' "$1" ;;
    esac
}

have_cmd() {
    command -v "$1" >/dev/null 2>&1
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

check_cmd() {
    label=$1
    name=$2
    required=$3

    if have_cmd "$name"; then
        ok "$label: $name ($(command -v "$name"))"
    elif [ "$required" = required ]; then
        missing_required "$label: не найден $name"
    else
        missing_optional "$label: не найден $name (опционально)"
    fi
}

check_pkg() {
    label=$1
    module=$2
    required=$3

    if ! have_cmd pkg-config; then
        missing_optional "$label: не проверено (нет pkg-config)"
        return
    fi

    if pkg-config --exists "$module"; then
        ok "$label: pkg-config $module"
    elif [ "$required" = required ]; then
        missing_required "$label: нет pkg-config-модуля $module"
    else
        missing_optional "$label: нет pkg-config-модуля $module (опционально)"
    fi
}

while IFS='|' read -r category kind name required description; do
    case "$category" in
        ''|'#'*)
            continue
            ;;
    esac

    [ -n "$kind" ] || continue
    [ -n "$name" ] || continue

    if [ -n "$only" ] && [ "$category" != "$only" ]; then
        continue
    fi

    if [ "$category" != "$last" ]; then
        printf '\n== %s ==\n' "$(section_title "$category")"
        last=$category
    fi

    if [ "$required" = conditional ]; then
        if [ -n "${DISPLAY:-}" ]; then
            ok "$description: не требуется (DISPLAY=$DISPLAY)"
            continue
        fi
        required=required
    fi

    case "$kind" in
        cmd)
            check_cmd "$description" "$name" "$required"
            ;;
        pkg)
            check_pkg "$description" "$name" "$required"
            ;;
        *)
            missing_optional "$description: неизвестный тип проверки $kind"
            ;;
    esac
done < "$MANIFEST"

printf '\n'
if [ "$fail" -ne 0 ]; then
    printf 'Итог: не хватает %i обязательных зависимостей\n' "$fail"
    exit 1
fi

printf 'Итог: обязательные зависимости на месте\n'
