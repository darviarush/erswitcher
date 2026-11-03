#!/usr/bin/env tclsh

# Функциональные тесты окна конфигуратора (Tcl/Tk).
#
# Запуск:
#   make test-functional
#   ./tests/functional/run.tcl
#   xvfb-run -a ./tests/functional/run.tcl   # без настоящего дисплея
#
# Скрипт создаёт песочницу с подменённым HOME, пишет туда тестовый
# erswitcher.conf, загружает сам конфигуратор и выполняет тесты из
# configurator.test.tcl. Так тесты не трогают настоящий конфиг пользователя
# и не посылают сигнал живому erswitcher-у.

package require tcltest

# Без X11 функциональные тесты выполнить нельзя: Tk не создаст окно.
# Это ошибка, а не пропуск — иначе прогон был бы ложно зелёным.
# В headless-окружении запускайте через xvfb-run (см. Makefile).
if {[catch {package require Tk} err]} {
	puts stderr "FAIL: Tk недоступен, функциональные тесты не выполнены ($err)"
	exit 1
}

namespace import ::tcltest::*

set here [file dirname [file normalize [info script]]]
set root [file normalize [file join $here ../..]]
set app  [file join $root erswitcher-configurator.tcl]

if {![file exists $app]} {
	puts stderr "FAIL: не найден $app"
	exit 1
}

# Песочница: HOME подменяется до запуска конфигуратора, потому что он
# читает ~/.config/erswitcher.conf и ~/.config/erswitcher-configurator.conf.
set tmpdir [expr {[info exists env(TMPDIR)] && $env(TMPDIR) ne ""
	? $env(TMPDIR) : "/tmp"}]
set ::ERS_HOME [file join $tmpdir "erswitcher-functional-[pid]"]
file delete -force $::ERS_HOME
file mkdir [file join $::ERS_HOME .config]

set env(HOME) $::ERS_HOME
cd $::ERS_HOME

set ::ERS_CONFIG [file join $::ERS_HOME .config erswitcher.conf]

# Конфиг, на котором проверяется вся раскраска. Строки намеренно
# пронумерованы ниже в тестах: 1 — закладка, 3/6/10 — секции,
# 7/8 — дубликат ключа, 11/12 — escape-последовательности, 13 — ошибка.
# Фигурные скобки сохраняют обратные слэши как есть.
set ::ERS_FIXTURE [string trim {
## Тестовая секция
# обычный комментарий
[functions]
Pause=word translate

[options]
delay=0,01
delay=0,02

[compose]
snippet=раз\^два
tab=\tи\n
bad line
}]

proc ::ers_write {path text} {
	set f [open $path w]
	puts -nonewline $f $text
	close $f
}

::ers_write $::ERS_CONFIG "$::ERS_FIXTURE\n"

# Конфигуратор исполняется на верхнем уровне: он создаёт окно . и
# раскладывает виджеты .m / .l / .t.
source $app
update idletasks

::tcltest::configure -verbose {fail error}

source [file join $here configurator.test.tcl]

# cleanupTests в Tk-приложении сам вызывает exit (и всегда с кодом 0),
# поэтому статистику, очистку песочницы и код возврата формируем здесь.
set total   $::tcltest::numTests(Total)
set passed  $::tcltest::numTests(Passed)
set skipped $::tcltest::numTests(Skipped)
set failed  $::tcltest::numTests(Failed)

# После тестов окно уже не нужно: закрываем и убираем песочницу.
catch {destroy .}
file delete -force $::ERS_HOME

puts "run.tcl:\tTotal\t$total\tPassed\t$passed\tSkipped\t$skipped\tFailed\t$failed"
exit [expr {$failed > 0}]
