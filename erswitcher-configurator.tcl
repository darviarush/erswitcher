#!/usr/bin/wish

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
bind Entry <Insert> {}
bind Text <Insert> {}

bind Entry <<Paste>> {
	catch { %W delete sel.first sel.last }
	catch {
		%W insert insert [::tk::GetSelection %W CLIPBOARD]
		tk::EntrySeeInsert %W
	}
}

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

# при установке курсора меняем и позицию в тулбаре
rename ::tk::TextSetCursor ::tk::TextSetCursorOrig
proc ::tk::TextSetCursor {w pos} {
    ::tk::TextSetCursorOrig $w $pos
	event generate $w <<CursorChanged>>
}

rename ::tk::TextButton1 ::tk::TextSetButton1Orig
proc ::tk::TextButton1 {w x y} {
  ::tk::TextSetButton1Orig $w $x $y
  event generate $w <<CursorChanged>>
}

#@category инициализация
 
pack [make_scrolled_y [frame .t] [text .t.text -wrap word]] -fill both -expand 1


set path "~/.config/erswitcher.conf"
set conf [read_file $path]

proc coloring {text} {
	set pos [.t.text index insert]
	.t.text delete 1.0 end
	
	foreach line [split $text "\n"] {
		switch -regexp $line {
			^\s*# { .t.text insert end $line -fg #AFEEEE }
			^\s*\[.*\]\s*$ { .t.text insert end $line -fg #F08080 }
			= { 
				regexp (([^\\]|\\.)*)=(.*) $line -> a b
				.t.text insert end $a 
				.t.text insert end = -fg #1E90FF
				.t.text insert end $b 
			}
			default { .t.text insert end $line -bg #DC143C -fg white }
		}
	}
	
	tk::TextSetCursor .t.text $pos
	.t.text see $pos
	#focus .t.text
}

coloring $conf

#after 10 {focus .t.text}

bind .t.text <KeyRelease> {
	set $text [.t.text get 1.0 end-1c]
	write_file $path $text
	coloring $text
	set lines [split [exec ps aux] "\n"]
	foreach line $lines {
		
	}
}

