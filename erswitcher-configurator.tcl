#!/usr/bin/wish

package require Tk

# делаем вертикальный скроллбар. f - фрейм с виджетом w для которого скроллбар и делается 
proc make_scrolled_y {f w} {
	scrollbar $f.scrollbar  -orient vertical -width 10 -command "$w yview"
	$w configure -yscrollcommand "$f.scrollbar set"
	pack $f.scrollbar -side right -fill y
	pack $w -expand 1 -fill both
	return $f
}

pack [make_scrolled_y [frame .t] [text .t.text -wrap word]] -fill both -expand 1


set path "~/.config/erswitcher.conf"


proc read_file {} {
	set f [open $path r]
	
	close $f
}
