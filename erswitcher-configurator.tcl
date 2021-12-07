#!/usr/bin/env wish

package require Tk

tk appname "En-Ru Switcher Конфигуратор"

# центрируем окно
set width 600
set height 500
set x [expr { ( [winfo vrootwidth  .] - $width  ) / 2 }]
set y [expr { ( [winfo vrootheight .] - $height ) / 2 }]
puts ${width}x${height}+${x}+${y}
wm geometry . ${width}x${height}+${x}+${y}

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
 
pack [make_scrolled_y [frame .t] [text .t.text -wrap word -undo 1 -maxundo -1]] -fill both -expand 1

.t.text tag configure remark -foreground #008080
.t.text tag configure section -foreground #DC143C
.t.text tag configure equal -foreground #1E90FF
.t.text tag configure error -background #DC143C -foreground white

set path "~/.config/erswitcher.conf"
set conf [read_file $path]
.t.text insert end $conf

proc coloring {text} {
	foreach i {remark section equal error} {
		.t.text tag remove $i 1.0 end
	}
	
	set re {^((?:\\.|[^\\])*)=}
	
	set lineno 0
	foreach line [split $text "\n"] {
		incr lineno
		
		switch -regexp -- $line {
			{^\#} { .t.text tag add remark $lineno.0 $lineno.end }
			{^\[(functions|compose|snippets|commands|sendkeys)\]\s*$} { .t.text tag add section $lineno.0 $lineno.end }
			{^\s*$} {}
			{=} {
				regexp $re $line -> a
				set charno [string length $a]
				.t.text tag add equal $lineno.$charno $lineno.$charno+1c
			}
			default { .t.text tag add error $lineno.0 $lineno.end }
		}
	}	
}

coloring $conf

# колоризируем текст и отправляем erswitcher-у сигнал на пересчитывание конфига
bind .t.text <KeyRelease> {
	global conf
	set text [.t.text get 1.0 end-1c]
	if {$conf == $text} {return}
	set conf $text
	write_file $path $text
	coloring $text

	set res [catch {exec killall -HUP erswitcher} result]
	puts "Сигнал отправлен erswitcher-у с результатом: $res ($result)"
}

