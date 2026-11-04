#!/bin/sh
# Печатает цветную сводку покрытия по выбранному фильтру
# По умолчанию считаются только исходники, без tests/unit
set -eu

ROOT=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
FILTER=${1:-src/.*}
GCOVR=${GCOVR:-gcovr}

if ! command -v "$GCOVR" >/dev/null 2>&1; then
    printf 'FAIL: не найден %s; установите gcovr\n' "$GCOVR" >&2
    exit 1
fi

report_branch=$(cd "$ROOT" && "$GCOVR" -r . --filter "$FILTER" --txt-metric branch)
report_line=$(cd "$ROOT" && "$GCOVR" -r . --filter "$FILTER" --txt-metric line)

parse_total() {
    printf '%s\n' "$1" | awk '$1 == "TOTAL" { print $2, $3, $4; exit }'
}

branch=$(parse_total "$report_branch")
line=$(parse_total "$report_line")

if [ -z "$branch" ] || [ -z "$line" ]; then
    printf 'FAIL: gcovr не вернул TOTAL для фильтра %s\n' "$FILTER" >&2
    exit 1
fi

set -- $branch
branch_total=$1
branch_taken=$2
branch_cover=${3%\%}

set -- $line
line_total=$1
line_taken=$2
line_cover=${3%\%}

use_color=no
case ${COVERAGE_COLOR:-auto} in
    always|yes|1)
        use_color=yes
        ;;
    never|no|0)
        ;;
    *)
        if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
            use_color=yes
        fi
        ;;
esac

if [ "$use_color" = yes ]; then
    green=$(printf '\033[32m')
    yellow=$(printf '\033[33m')
    red=$(printf '\033[31m')
    bold=$(printf '\033[1m')
    reset=$(printf '\033[0m')
else
    green=
    yellow=
    red=
    bold=
    reset=
fi

color_for() {
    if [ "$1" -ge 80 ]; then
        printf '%s' "$green"
    elif [ "$1" -ge 60 ]; then
        printf '%s' "$yellow"
    else
        printf '%s' "$red"
    fi
}

bar_for() {
    pct=$1
    width=20
    filled=$((pct * width / 100))
    if [ "$filled" -gt "$width" ]; then
        filled=$width
    fi
    i=0
    printf '['
    while [ "$i" -lt "$filled" ]; do
        printf '#'
        i=$((i + 1))
    done
    while [ "$i" -lt "$width" ]; do
        printf '-'
        i=$((i + 1))
    done
    printf ']'
}

print_metric() {
    label=$1
    total=$2
    taken=$3
    pct=$4
    color=$(color_for "$pct")
    printf '  %-7s %s%s%s %s%3s%%%s  (%s/%s)\n' \
        "$label" \
        "$color" "$(bar_for "$pct")" "$reset" \
        "$color" "$pct" "$reset" \
        "$taken" "$total"
}

case "$FILTER" in
    'src/.*'|'src/*')
        title='Покрытие исходников (src)'
        ;;
    *)
        title="Покрытие по фильтру: $FILTER"
        ;;
esac

printf '%s%s%s\n' "$bold" "$title" "$reset"
print_metric 'ветви' "$branch_total" "$branch_taken" "$branch_cover"
print_metric 'строки' "$line_total" "$line_taken" "$line_cover"
