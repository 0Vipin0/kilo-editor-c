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
	// Turn off all output processing (OPOST)
	raw.c_oflag &= ~(OPOST);
	// Turn off Ctrl+S and Ctrl+Q Signals (IXON), Ctrl+M (ICRNL)
	raw.c_iflag &= ~(IXON | ICRNL);
	// Turn off Echo, Canonical, Ctrl+C/Ctrl+Z Signals (ISIG), Ctrl+V Signals(IEXTEN)
	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
	
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
			printf("%d\r\n", c);
		} else {
			printf("%d ('%c')\r\n", c, c);
		}
	}
	return 0;
}
