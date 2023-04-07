#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios orig_termios;

void disableRawMode(){
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode(){
	
	// Get the Terminal Data in to raw
	// Linux terminal input, output and errro mode
	// STDIN_FILENO -> 0
	// STDOUT_FILENO -> 1
	// STDERR_FILENO -> 2
	tcgetattr(STDIN_FILENO, &orig_termios);
	// Disable is automatically called when program exits either from main() or exit() to reset the terminal in original state
	atexit(disableRawMode);

	struct termios raw = orig_termios;
	// Turn off Ctrl+S and Ctrl+Q Signals (IXON)
	raw.c_iflag &= ~(IXON);
	// Turn off Echo, Canonical, Ctrl+C/Ctrl+Z Signals (ISIG)
	raw.c_lflag &= ~(ECHO | ICANON | ISIG);
	
	// Set the Terminal Data
	// TCSAFLUSH argument specifies when to apply the change, it waits for all pending output to be written to the terminal, discard any input that hasn't been read
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main() {
	// Turn the terminal to Raw mode from Canonical Mode
	enableRawMode();
	// Variable to get the input
	char c;
	// Read 1 byte from the standard input into 'c' until no bytes left to read or q key is entered
	while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q'){
		if(iscntrl(c)){
			printf("%d\n", c);
		} else {
			printf("%d ('%c')\n", c, c);
		}
	}
	return 0;
}
