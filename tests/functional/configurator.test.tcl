# Функциональные тесты окна конфигуратора.
#
# Файл подключается из run.tcl: к этому моменту конфигуратор уже
# загружен, окно . построено, а $::ERS_FIXTURE лежит в песочнице
# $::ERS_HOME/.config/erswitcher.conf.
#
# Тесты работают с настоящими виджетами Tk и настоящими событиями X11
# (event generate), а не с внутренними процедурами раскраски.

# Возвращает окно и текст к исходному состоянию. coloring перерисовывает
# теги и закладки, поэтому вызывается после каждой правки.
proc ::ers_fixture_text {} {
	.t.text delete 1.0 end
	.t.text tag remove sel 1.0 end
	.t.text insert 1.0 $::ERS_FIXTURE
	set ::conf $::ERS_FIXTURE
	coloring $::ERS_FIXTURE
	.l.list selection clear 0 end
	::ers_write $::ERS_CONFIG "$::ERS_FIXTURE\n"
	# Синтетические клавиатурные события доходят до виджета только
	# когда он в фокусе, а окно уже отображено.
	focus -force .t.text
	update
}

# exec подменяется на время теста правки: конфигуратор по KeyRelease
# делает `exec killall -HUP erswitcher`, а тест не должен трогать
# настоящий процесс.
proc ::ers_mock_exec_install {} {
	if {![info exists ::ers_real_exec]} {
		rename exec ::ers_real_exec
		proc exec {args} {
			lappend ::ers_exec_log $args
			return ""
		}
	}
	set ::ers_exec_log {}
}

proc ::ers_mock_exec_remove {} {
	if {[info exists ::ers_real_exec]} {
		rename exec {}
		rename ::ers_real_exec exec
	}
}

#@category окно

test window-layout {панели, виджеты и их прокрутка созданы} -setup {
	::ers_fixture_text
} -body {
	list \
		[winfo class .m] \
		[lsort [.m panes]] \
		[winfo class .l.list] \
		[winfo class .t.text] \
		[winfo exists .l.scrollbar] \
		[winfo exists .t.scrollbar]
} -result {Panedwindow {.l .t} Listbox Text 1 1}

test window-scrollbars {скроллбары привязаны к списку и тексту} -setup {
	::ers_fixture_text
} -body {
	list \
		[expr {[.l.list cget -yscrollcommand] eq ".l.scrollbar set"}] \
		[expr {[.t.text cget -yscrollcommand] eq ".t.scrollbar set"}] \
		[.l.scrollbar cget -orient] \
		[.t.scrollbar cget -orient]
} -result {1 1 vertical vertical}

test window-text-options {текст редактируется с undo и переносом слов} -setup {
	::ers_fixture_text
} -body {
	list [.t.text cget -wrap] [.t.text cget -undo] [.t.text cget -maxundo]
} -result {word 1 -1}

test window-geometry {окно получает размер по умолчанию 800x500} -body {
	expr {[regexp {^800x500[+-][0-9]+[+-][0-9]+$} [wm geometry .]]}
} -result 1

test window-close-protocol {на закрытие окна назначен обработчик} -body {
	list \
		[expr {[wm protocol . WM_DELETE_WINDOW] ne ""}] \
		[expr {[string first write_file [wm protocol . WM_DELETE_WINDOW]] >= 0}]
} -result {1 1}

#@category загрузка конфига

test config-load {конфиг загружен в текстовое поле без хвостового перевода строки} -body {
	list \
		[expr {[.t.text get 1.0 end-1c] eq $::ERS_FIXTURE}] \
		[expr {$::conf eq $::ERS_FIXTURE}]
} -result {1 1}

test config-bookmarks {закладки собраны из секций и заголовков ##} -setup {
	::ers_fixture_text
} -body {
	list [.t.text tag ranges bookmark] $::bookmarks [join [.l.list get 0 end] "|"]
} -result {{1.0 1.18} {1 3 6 10} {Тестовая секция|[functions]|[options]|[compose]}}

#@category раскраска

test coloring-sections {секции выделены цветом} -setup {
	::ers_fixture_text
} -body {
	.t.text tag ranges section
} -result {3.0 3.11 6.0 6.9 10.0 10.9}

test coloring-comments {комментарии и знак равенства подсвечены} -setup {
	::ers_fixture_text
} -body {
	list [.t.text tag ranges remark] [.t.text tag ranges equal]
} -result {{2.0 2.21} {4.5 4.6 7.5 7.6 8.5 8.6 11.7 11.8 12.3 12.4}}

test coloring-error {строка без = помечена как ошибка} -setup {
	::ers_fixture_text
} -body {
	list [.t.text tag ranges error] [.t.text tag ranges warn]
} -result {{13.0 13.8} {7.0 7.5 8.0 8.5}}

test coloring-duplicate-keys {повтор ключа подсвечен предупреждением} -setup {
	::ers_fixture_text
} -body {
	.t.text tag ranges warn
} -result {7.0 7.5 8.0 8.5}

test coloring-escapes {\^ красится как курсор, \t и \n как escape} -setup {
	::ers_fixture_text
} -body {
	list [.t.text tag ranges cursor] [.t.text tag ranges backslash]
} -result {{11.11 11.13} {12.4 12.6 12.7 12.9}}

test coloring-reset {повторная раскраска снимает старые теги и закладки} -setup {
	::ers_fixture_text
} -body {
	set text "## Другая\n\[options\]\nx=1"
	.t.text delete 1.0 end
	.t.text insert 1.0 $text
	coloring $text
	list \
		[llength [.t.text tag ranges error]] \
		[llength [.t.text tag ranges warn]] \
		$::bookmarks \
		[join [.l.list get 0 end] "|"]
} -result {0 0 {1 2} {Другая|[options]}}

#@category события

test event-select-all {Control+KeyRelease+a выделяет весь текст} -setup {
	::ers_fixture_text
} -body {
	event generate .t.text <Control-KeyRelease-a> -when now
	update idletasks
	list [expr {[llength [.t.text tag ranges sel]] > 0}] [.t.text tag ranges sel]
} -result {1 {1.0 14.0}}

test event-edit-saves-config {правка текста сохраняется в конфиг и шлёт сигнал} -setup {
	::ers_fixture_text
	::ers_mock_exec_install
} -cleanup {
	::ers_mock_exec_remove
	::ers_fixture_text
} -body {
	.t.text insert end "\n\[snippets\]\nSuper+Pause=Готово."
	set expected [.t.text get 1.0 end-1c]

	focus -force .t.text
	update
	event generate .t.text <KeyRelease> -when now
	update idletasks

	set saved [read_file $::ERS_CONFIG]
	list \
		[expr {[string trim $saved] eq $expected}] \
		[expr {[string index $saved end] eq "\n"}] \
		[expr {$::conf eq $expected}] \
		$::ers_exec_log \
		[lindex $::bookmarks end] \
		[.l.list get end]
} -result {1 1 1 {{killall -HUP erswitcher}} 14 {[snippets]}}

test event-bookmark-jumps {выбор закладки прокручивает текст к её строке} -setup {
	::ers_fixture_text
} -cleanup {
	::ers_fixture_text
} -body {
	set lines {}
	for {set i 1} {$i <= 80} {incr i} {
		lappend lines "# строка $i"
	}
	lappend lines "## Конец"
	set text [join $lines "\n"]

	.t.text delete 1.0 end
	.t.text insert 1.0 $text
	set ::conf $text
	coloring $text
	update idletasks

	.l.list selection clear 0 end
	.l.list selection set end
	event generate .l.list <<ListboxSelect>>
	update idletasks

	list \
		[expr {[lindex [.t.text yview] 0] > 0}] \
		$::bookmarks \
		[.l.list curselection]
} -result {1 81 0}

#@category файловые помощники

test file-helpers {exists_file/read_file/write_file работают с диском} -body {
	set dir [file join $::ERS_HOME files]
	file mkdir $dir
	set path [file join $dir sample.conf]

	set before [exists_file $path]
	write_file $path "one\ntwo"
	list \
		$before \
		[expr {[read_file $path] eq "one\ntwo\n"}] \
		[exists_file $path] \
		[exists_file [file join $dir missing.conf]]
} -result {0 1 1 0}
