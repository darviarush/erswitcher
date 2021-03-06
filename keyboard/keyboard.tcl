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
		frame $x.$k -bg #FFF -bd 1  -relief raised
		label $x.$k.lab_num -text $k -width 10 -fg #F00
		label $x.$k.lab_sym -text [lindex [lindex $keyboard $k] 4] -width 10
		grid $x.$k.lab_num -row 0 -column 0 -in $x.$k
		grid $x.$k.lab_sym -row 1 -column 0 -in $x.$k
		
		grid $x.$k -row $row -column $col -in $w
		
		incr col
	}
}

proc from {a to b} {
	set r []
	for {set i $a} {$i <= $b} {incr i} {lappend r $i}
	return $r
}

proc keyboard {w keyboard with_null with_prev} {
	if {$w == "."} {set x ""} {set x $w}
	
	array set k_span {
		9 4
						22 5
		23 4			51 4
		66 5			36 5
		50 6			62 6
		37 4	65 16
	}
		
	set kbd "
		{ 9   [from 67 to 76] 95 96   107 78 127}
		{49   [from 10 to 22]  118 110 112}
		{ [from 23 to 35] 51   119 115 116}
		{66 [from 38 to 48] 36 }
		{50 [from 52 to 62]                     -1  111 }
		{37 207 133 64   65   108 134 135 105   113 116 114 }
	"
		
	set row 0
	
	foreach rowset $kbd {
		
		set col 0
		
		foreach k $rowset {
		
			if {$k == -1} {incr col 3; continue}
		
			set kb [lindex $keyboard $k]
			set prev_sym ""
			set frame $x.key${row}_$col
			# raised, sunken, flat, ridge, solid, and groove
			frame $frame -bd 1 -relief raised
	
			set langs [expr {([llength $kb] - 1) / 10}]
	
			for {set i 0} {$i < $langs} {incr i} {
			
				set i_kb [expr 1+$i*10]
				
				for {set j 0} {$j < 2} {incr j} {
					set shift_i [expr $i_kb+2+4*$j]
				
					set name [lindex $kb [expr $shift_i+1]]
					set sym [lindex $kb [expr $shift_i+2]]
					set sym_num [lindex $kb [expr $shift_i+3]]
				
					if {$sym == "␀"} {set sym $name}

					if {!$with_prev && $prev_sym == $sym} {continue}
					if {$sym == "(null)"} { if {$with_null} {set sym "∅"} {continue} }
					
					label $frame.lab${i}_$j -text $sym
				
					grid $frame.lab${i}_$j -row $i -column $j -in $frame
					set prev_sym $sym
				}
			}
		
			set span 3
			catch {set span $k_span($k)}
			
			grid $frame -column $col -row $row -columnspan $span -sticky "nsew"
			incr col $span
		}
		incr row
		
	}
}

#ancient_keyboard . $keyboard
keyboard . $keyboard 0 0

