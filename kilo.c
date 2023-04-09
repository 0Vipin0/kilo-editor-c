/*** includes ***/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

/*** defines ***/
// Sets the upper 3 bits of character to 0 like Ctrl key does
#define CTRL_KEY(k) ((k) & 0x1f) 

/*** data ***/

struct termios orig_termios;

void die(const char *s){
	perror(s);
	exit(1);
}

/*** terminal ***/

void disableRawMode(){
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
		die("tcsetattr");
}

void enableRawMode(){
	
	// Get the Terminal Data in to raw
	// Linux terminal input, output and errro mode
	// STDIN_FILENO -> 0
	// STDOUT_FILENO -> 1
	// STDERR_FILENO -> 2
	if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
		die("tcgetattr");
	// Disable is automatically called when program exits either from main() or exit() to reset the terminal in original state
	atexit(disableRawMode);

	struct termios raw = orig_termios;
	// Turn off all output processing (OPOST)
	raw.c_oflag &= ~(OPOST);
	// Turn off Ctrl+S and Ctrl+Q Signals (IXON), Ctrl+M (ICRNL), Misc. flags(BRKINT - cause a SIGINT signal to program, INPCK - Enable Parity Checking, so not relevant to modern terminals, ISTRIP - 8th bit of each input set to 0)
	raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
	// Turn off Echo, Canonical, Ctrl+C/Ctrl+Z Signals (ISIG), Ctrl+V Signals(IEXTEN)
	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
	// Bit Mask - Set character size to 8 bits 
	raw.c_cflag |= (CS8);
	// VMIN - Set Minimum Number of bytes of input needed before read() return
	raw.c_cc[VMIN] = 0;
	// VMAX - Set Maximum Time amount to wait before read() returns
	raw.c_cc[VTIME] = 1;

	// Set the Terminal Data
	// TCSAFLUSH argument specifies when to apply the change, it waits for all pending output to be written to the terminal, discard any input that hasn't been read
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
		die("tcsetattr");
}

char editorReadKey(){
	// Used for error checking
	int nread;
	// Character to be read in to c
	char c;
	while( (nread = read(STDIN_FILENO, &c, 1)) != 1){
		if(nread == -1 && errno != EAGAIN) die("read");
	}
	return c;
}

/*** input ***/

void editorProcessKeypress(){
	char c = editorReadKey();

	switch(c){
		case CTRL_KEY('q'):
			exit(0);
			break;
	}
}


/*** init ***/

int main() {
	// Turn the terminal to Raw mode from Canonical Mode
	enableRawMode();
	// Read 1 byte from the standard input into 'c' until no bytes left to read or q key is entered
	while (1){
		editorProcessKeypress();
	}
	return 0;
}
