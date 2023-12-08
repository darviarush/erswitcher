#!/usr/bin/env wish

package require Tk

tk appname "En-Ru Switcher Конфигуратор"

#@category виджеты

# делаем вертикальный скроллбар. f - фрейм с виджетом w для которого скроллбар и делается
proc make_scrolled_y {f w} {
	scrollbar $f.scrollbar  -orient vertical -width 10 -command "$w yview"
	$w configure -yscrollcommand "$f.scrollbar set"
	pack $f.scrollbar -side right -fill y
	pack $w -expand 1 -fill both
	return $f
}


#@category файлы

proc exists_file {path} {
	return [expr {![catch {file lstat $path finfo}]}]
}

proc read_file {path} {
	set f [open $path r]
	set file_data [read $f]
	close $f
	return $file_data
}

proc write_file {path text} {
	set f [open $path w]
	puts $f $text
	close $f
}

#@category редактирование
# bind Entry <Insert> {}
bind Text <Insert> {}

# bind Entry <<Paste>> {
	# catch { %W delete sel.first sel.last }
	# catch {
		# %W insert insert [::tk::GetSelection %W CLIPBOARD]
		# tk::EntrySeeInsert %W
	# }
# }

proc ::tk_textPaste w {
    if {![catch {::tk::GetSelection $w CLIPBOARD} sel]} {
		set oldSeparator [$w cget -autoseparators]
		if {$oldSeparator} {
		    $w configure -autoseparators 0
		    $w edit separator
		}
		catch { $w delete sel.first sel.last }
		$w insert insert $sel
		if {$oldSeparator} {
		    $w edit separator
		    $w configure -autoseparators 1
		}
    }
}

# # при установке курсора меняем и позицию в тулбаре
# rename ::tk::TextSetCursor ::tk::TextSetCursorOrig
# proc ::tk::TextSetCursor {w pos} {
    # ::tk::TextSetCursorOrig $w $pos
	# event generate $w <<CursorChanged>>
# }

# rename ::tk::TextButton1 ::tk::TextSetButton1Orig
# proc ::tk::TextButton1 {w x y} {
  # ::tk::TextSetButton1Orig $w $x $y
  # event generate $w <<CursorChanged>>
# }

#@category инициализация

pack [panedwindow .m -orient horizontal] -fill both -expand 1

pack [make_scrolled_y [frame .l] [listbox .l.list -selectmode single]] -side left -fill y
pack [make_scrolled_y [frame .t] [text .t.text -wrap word -undo 1 -maxundo -1]] -fill both -expand 1

.m add .l
.m add .t

set colors {
	bookmark 	#DC143C
	remark 		#008080
	section 	#DC143C
	equal 		#1E90FF
	error 		#DC143C
	warn 		#CD5C5C
	cursor 		#4169E1
	backslash 	#228B22
}

set bgcolors {error warn}

# set bg [.t.text cget -background]

dict for {key fg} $colors {
	if { $key in $bgcolors } {
		.t.text tag configure $key -background $fg -foreground white
	} else {
		.t.text tag configure $key -foreground $fg
	}
}
# .t.text tag configure bookmark -foreground #DC143C
# .t.text tag configure remark -foreground #008080
# .t.text tag configure section -foreground #DC143C
# .t.text tag configure equal -foreground #1E90FF
# .t.text tag configure error -background #DC143C -foreground white
# .t.text tag configure warn -background #CD5C5C -foreground white
# .t.text tag configure cursor -foreground #4169E1
# .t.text tag configure backslash -foreground #228B22

set bookmarks []
proc add_bookmark {title lineno} {
	global bookmarks
	.l.list insert end $title
	lappend bookmarks $lineno
}
proc clear_bookmarks {} {
	global bookmarks
	set bookmarks []
	.l.list delete 0 end
}


# центрируем окно
set path_wmconf "~/.config/erswitcher-configurator.conf"
set wmconf ""
catch {set wmconf [read_file $path_wmconf]}

if {$wmconf == ""} {
	set width 800
	set height 500
	set x [expr { ( [winfo vrootwidth  .] - $width  ) / 2 }]
	set y [expr { ( [winfo vrootheight .] - $height ) / 2 }]
	puts ${width}x${height}+${x}+${y}
	wm geometry . ${width}x${height}+${x}+${y}
} else {
	regexp {^(\d+) (.*?)\n} $wmconf -> width geometry

	wm geometry . $geometry
	.m paneconfigure .l -width $width
}

wm protocol . WM_DELETE_WINDOW {
	write_file $path_wmconf "[winfo width .l] [wm geometry .]"
	destroy .
}

# показываем конфиг
set path "~/.config/erswitcher.conf"
set conf [string trim [read_file $path]]
.t.text insert end $conf

# колоризирует код и переписывает закладки
proc coloring {text} {
	global colors
	# удаляем теги
	dict for {color_name fg} $colors {
		.t.text tag remove $color_name 1.0 end
	}
	# удаляем закладки
	clear_bookmarks

	# множество опций, если ключ встречается несколько раз, то все вхождения подсвечиваются оранжевым 
	array set keys []

	set re {^((?:\\.|[^\\])*?)=}

	set lineno 0
	foreach line [split $text "\n"] {
		incr lineno

		switch -regexp -- $line {
			{^## } {
				# закладки
				set is [regexp {^\#\#\s+(.+)} $line -> s]
				if {$is} {
					add_bookmark $s $lineno
				}
				# подсветка комментария в тексте
				.t.text tag add bookmark $lineno.0 $lineno.end
			}
			{^\#} {	.t.text tag add remark $lineno.0 $lineno.end }
			{^\[(functions|options|compose|snippets|commands|sendkeys)\]\s*$} { 
				.t.text tag add section $lineno.0 $lineno.end
				add_bookmark $line $lineno
			}
			{^\s*$} {}
			{=} {
				regexp $re $line -> key

				set charno [string length $key]
				.t.text tag add equal $lineno.$charno $lineno.$charno+1c

				if {[info exists keys($key)]} {
					.t.text tag add warn {*}$keys($key)
					.t.text tag add warn $lineno.0 $lineno.$charno
				} else {
					set keys($key) "$lineno.0 $lineno.$charno"
				}

				foreach {m} [regexp -start $charno -indices -all -inline {\\[nt^]} $line] {
					set i [lindex $m 0]
					set j [lindex $m 1]

					if {[string range $line $i $j] == "\\^"} {
						set backslash cursor
					} else {
						set backslash backslash
					}
					.t.text tag add $backslash $lineno.$i $lineno.[expr $j+1]
				}

			}
			default { .t.text tag add error $lineno.0 $lineno.end }
		}
	}
}

coloring $conf

# выделяем всё
bind .t.text <Control-KeyRelease-a> {
	#puts "Control-KeyRelease-a"
	%W tag add sel 1.0 end
}

# колоризируем текст и отправляем erswitcher-у сигнал на пересчитывание конфига
# bind .t.text <KeyPress> {
# 	puts "KeyPress: A=%A K=%K k=%k N=%N"
# }

bind .t.text <KeyRelease> {
	global conf

	#puts "KeyRelease: A=%A K=%K k=%k N=%N"

	set text [.t.text get 1.0 end-1c]
	set text [string trim $text]
	if {$conf == $text} {
		return
	}
	set conf $text
	write_file $path "$text\n"
	coloring $text

	set res [catch {exec killall -HUP erswitcher} result]
	puts "Сигнал отправлен erswitcher-у с результатом: $res ($result)"
}

# при выборе закладки переходим к ней
bind .l.list <<ListboxSelect>> {
	set idx [%W curselection]
	if {$idx != ""} {
		set pos [lindex $bookmarks $idx]
		.t.text see "$pos.0"
	}
}
