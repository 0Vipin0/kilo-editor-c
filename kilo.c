/*** includes ***/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

/*** defines ***/

// Sets the upper 3 bits of character to 0 like Ctrl key does
#define CTRL_KEY(k) ((k) & 0x1f) 

/*** data ***/

struct editorConfig {
	int screenrows;
	int screencols;
	struct termios orig_termios;
};

struct editorConfig E;

void die(const char *s){
	// Clear screen on exit
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);

	perror(s);
	exit(1);
}

/*** terminal ***/

void disableRawMode(){
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
		die("tcsetattr");
}

void enableRawMode(){
	
	// Get the Terminal Data in to raw
	// Linux terminal input, output and errro mode
	// STDIN_FILENO -> 0
	// STDOUT_FILENO -> 1
	// STDERR_FILENO -> 2
	if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
		die("tcgetattr");
	// Disable is automatically called when program exits either from main() or exit() to reset the terminal in original state
	atexit(disableRawMode);

	struct termios raw = E.orig_termios;
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

int getCursorPosition(int *rows, int *cols){
	char buf[32];
	unsigned int i = 0;

	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

	while (i < sizeof(buf) - 1){
		if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
		if (buf[i]) break;
		i++;
	}

	buf[i] = '\0';

	if (buf[0] != '\x1b' || buf[1] != '[') return -1;
	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

	return 0;
}

int getWindowSize(int *rows, int *cols){
	// From sys/ioctl
	struct winsize ws;
	// TIOCGWINSZ -> Terminal IOCtl (which itself stands for Input/Output Control) Get WINdow SiZe)
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
		// C -> moves cursor to right, B -> moves cursor to down
		if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
		return getCursorPosition(rows, cols);
	} else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

/*** output ***/

void editorDrawRows(){
	int i = 0;
	for( i = 0; i < E.screenrows ; i++){
		write(STDOUT_FILENO, "~", 1);
		// If this is not the last row, then return carriage and print new lien -> To make sure the last line have tilde
		if(i < E.screenrows - 1){
			write(STDOUT_FILENO, "\r\n", 2);
		}
	}
}

void editorRefreshScreen(){
	// \x1b -> Escape Character (27), \x1b[ -> Escape Sequence, J -> Clear Screen, 2 -> Clear Entire Screen
	// write -> write 4 bytes to screen
	write(STDOUT_FILENO, "\x1b[2J", 4);
	// Escape Sequence -> Reposition Cursor (Default Position - 1,1)
	write(STDOUT_FILENO, "\x1b[H", 3);

	editorDrawRows();

	write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** input ***/

void editorProcessKeypress(){
	char c = editorReadKey();

	switch(c){
		case CTRL_KEY('q'):
			// Clear the screen on exit
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;
	}
}


/*** init ***/

void initEditor(){
	if (getWindowSize(&E.screenrows, &E.screencols) == -1 ) die ("getWindowSize");
}

int main() {
	// Turn the terminal to Raw mode from Canonical Mode
	enableRawMode();
	initEditor();
	// Read 1 byte from the standard input into 'c' until no bytes left to read or q key is entered
	while (1){
		editorRefreshScreen();
		editorProcessKeypress();
	}
	return 0;
}
