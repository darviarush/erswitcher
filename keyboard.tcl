#!/usr/bin/wish

package require Tk

tk appname "Keyboard"
#wm geometry . ""

# Клавиатура представляет собой таблицу. 
# Стандартные клавиши занимают 3 colspan-а.

set keyboard [list]

set f [open keyboard.csv]

while {[gets $f line] >= 0} {
	lappend keyboard [split $line "\t"]
}
close $f


proc ancient_keyboard {w keyboard} {
	
	if {$w == "."} {set x ""} {set x $w}

	set row 0
	set col 0
	for {set k 0} {$k < [llength $keyboard]} {incr k} {

		if {$k == 9 || $k == 23 || $k == 37 || $k == 50 || $k == 63 || $k > 63 && [expr {($k - 63) % 14}] == 0} {
			set col 0
			incr row
		}

		# raised, sunken, flat, ridge, solid, and groove
		frame $x.$k -bg white -bd 1  -relief raised
		label $x.$k.lab -text [lindex [lindex $keyboard $k] 4] -width 10
		grid $x.$k.lab -row 0 -column 0 -in $x.$k
		
		grid $x.$k -row $row -column $col -in $w
		
		incr col
	}
}

proc keyboard {$w keyboard} {
	
}

ancient_keyboard . $keyboard