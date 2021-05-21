


#open f, "/dev/input/by-id/usb-Logitech_USB_Keyboard-event-kbd" or die $!;
open f, "/dev/console" or die $!;

while(sysread f, $c, 1) {
	print ord($c), " $c\n";
}