package ERSwitcher;
# переключатель клавиатуры и транслитератор последних введённых символов

use common::sense;
use X11::Xlib;
use Time::HiRes qw/usleep/;

# конструктор
sub new {
	my ($cls) = @_;
	bless {
        interval => 10_000,
    }, ref $cls || $cls;
}

# обработчик ошибки
sub x_error {
	my ($display, $e) = @_;
	::p($e);
}

# открывает дисплей
sub open {
	my ($self) = @_;
	$self->{d} = X11::Xlib->new(":0");
    $self->d->on_error(\&x_error);
    $self
}

# возвращает дисплей
sub d {
	my ($self) = @_;
	$self->{d}
}

# сконирует клавиатуру и возвращает нажатые клавиши в виде  или undef
sub scan {
	my ($self) = @_;
	#return {keymap=> $self->d->keymap, km=>$self->d->keymap->keymap, XQueryKeymap=>X11::Xlib::XQueryKeymap($self->d)};
    
    my $save;
    
    while() {
        my $keys = $self->d->XQueryKeymap;
        usleep($self->{interval}), next if $keys == $save;
        
		$self->key_on;
		
        # for(my $scancode = 0; $scancode<32*8; $scancode++) {
			# next if vec($keys, $scancode) == vec($save, $scancode);
			
			# if(vec($keys, $scancode)) { # нажата
				# $self->key_on($scancode);
			# } else { # отжата
				
			# }
		# }
	
		$save = $keys;
    }
}

# вызывается по событию - нажата клавиша
sub key_on {
	my ($self) = @_;
	
	my $keymap = $self->d->keymap;
	
	
	
	::p($keymap->modmap);
	
	$self
}



1;