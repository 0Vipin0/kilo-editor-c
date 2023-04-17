/*** includes ***/
// To manage warning 'implicit declaration of function ‘getline’' below define is used 
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>

/*** defines ***/

#define KILO_VERSION "0.0.1"

// Sets the upper 3 bits of character to 0 like Ctrl key does
#define CTRL_KEY(k) ((k) & 0x1f) 

enum editorKey {
	ARROW_LEFT = 1000,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PAGE_UP,
	PAGE_DOWN
};

/*** data ***/

typedef struct erow {
	int size;
	char *chars;
} erow;

struct editorConfig {
	int cx, cy;
	int rowoff;
	int coloff;
	int screenrows;
	int screencols;
	int numrows;
	erow *row;
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

int editorReadKey(){
	// Used for error checking
	int nread;
	// Character to be read in to c
	char c;
	while( (nread = read(STDIN_FILENO, &c, 1)) != 1){
		if(nread == -1 && errno != EAGAIN) die("read");
	}
	
	if (c == '\x1b'){
		char seq[3];

		if (read (STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
		if (read (STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

		if (seq[0] == '['){
			if (seq[1] >= '0' && seq[1] <= '9') {
				if (read(STDIN_FILENO, &seq[2], 1)!= 1) return '\x1b';
				if (seq[2] == '~'){
					switch(seq[1]){
						case '1' : return HOME_KEY;
						case '3' : return DEL_KEY;
						case '4' : return END_KEY;
						case '5' : return PAGE_UP;
						case '6' : return PAGE_DOWN;
						case '7' : return HOME_KEY;
						case '8' : return END_KEY;
					}
				}
			} else {
				switch (seq[1]){
					case 'A' : return ARROW_UP;
					case 'B' : return ARROW_DOWN;
					case 'C' : return ARROW_RIGHT;
					case 'D' : return ARROW_LEFT;
					case 'H' : return HOME_KEY;
					case 'F' : return END_KEY;
				}
			}
		} else if(seq[0] == 'O') {
			switch (seq[1]){
				case 'H' : return HOME_KEY;
				case 'F' : return END_KEY;
			}
		}
		return '\x1b';
	} else {
		return c;
	}
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

/*** row operations ***/

void editorAppendRow(char *s, size_t len){
	E.row = realloc(E.row, sizeof(erow) * (E.numrows+1));

	int at = E.numrows;
	E.row[at].size = len;
	E.row[at].chars = malloc(len +1);
	memcpy(E.row[at].chars, s, len);
	E.row[at].chars[len] = '\0';
	E.numrows++;
}

/*** file i/o ***/

void editorOpen(char *filename){
	FILE *fp = fopen(filename, "r");
	if(!fp) die("fopen");

	char *line = NULL;
	size_t linecap = 0;
	ssize_t linelen;

	linelen = getline(&line, &linecap, fp);
	while((linelen = getline(&line, &linecap, fp)) != -1){
		while( linelen > 0 && (line[linelen-1] == '\n' || line[linelen - 1] == '\r')) linelen--;
		editorAppendRow(line, linelen);
	}
	free(line);
	fclose(fp);
}

/*** append buffer ***/

struct abuf {
	char *b;
	int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len){
	char *new = realloc(ab->b, ab->len+len);

	if(new == NULL) return;
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}

void abFree(struct abuf *ab){
	ab->b = NULL;
	ab->len = 0;
	//free(ab);
}

/*** output ***/

void editorScroll(){
	if(E.cy < E.rowoff){
		E.rowoff = E.cy;
	}
	if(E.cy >= E.rowoff + E.screenrows){
		E.rowoff = E.cy - E.screenrows + 1;
	}
	if(E.cx < E.coloff){
		E.coloff = E.cx;
	}
	if(E.cx >= E.coloff + E.screencols){
		E.coloff = E.cx - E.screencols + 1;
	}
}

void editorDrawRows(struct abuf *ab){
	int i = 0;
	for( i = 0; i < E.screenrows ; i++){
		int fileRow = i + E.rowoff;
		if (fileRow >= E.numrows){
			if (E.numrows == 0 && i == E.screenrows /3){
				char welcome[80];
				int welcomelen = snprintf(welcome,sizeof(welcome),"Kilo Editor -- version %s", KILO_VERSION);
				if (welcomelen > E.screencols) welcomelen = E.screencols;
				int padding = (E.screencols - welcomelen) / 2;
				if (padding){
					abAppend(ab, "~",1);
					padding --;
				}
				while (padding--) abAppend(ab, " ", 1);
				abAppend(ab, welcome, welcomelen);
			} else {
				abAppend(ab, "~", 1);
			}
		} else {
			int len = E.row[fileRow].size - E.coloff;
			if (len < 0) len = 0;
			if (len > E.screencols) len = E.screencols;
			abAppend(ab, &E.row[fileRow].chars[E.coloff], len);
		}
		
		// Erases part of line to the right of current position
		abAppend(ab, "\x1b[K", 3);

		// If this is not the last row, then return carriage and print new lien -> To make sure the last line have tilde
		if(i < E.screenrows - 1){
			abAppend(ab, "\r\n", 2);
		}
	}
}

void editorRefreshScreen(){
	editorScroll();

	struct abuf ab = ABUF_INIT;
	// \x1b -> Escape Character (27), \x1b[ -> Escape Sequence, J -> Clear Screen, 2 -> Clear Entire Screen
	// l Command -> Reset Mode
	abAppend(&ab, "\x1b[?25l", 6);
	// Escape Sequence -> Reposition Cursor (Default Position - 1,1)
	abAppend(&ab, "\x1b[H", 3);

	editorDrawRows(&ab);

	char buf[32];
	snprintf(buf,sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff)+1, (E.cx - E.coloff)+1);
	abAppend(&ab, buf, strlen(buf));

	abAppend(&ab, "\x1b[H", 3);

	// h Command -> Set Mode
	abAppend(&ab, "\x1b[?25h", 6);

	write(STDOUT_FILENO, ab.b, ab.len);

	abFree(&ab);
}

/*** input ***/

void editorMoveCursor(int key){
	erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

	switch(key){
		case ARROW_LEFT:
			if (E.cx != 0){
				E.cx--;
			} else if (E.cy > 0){
				E.cy--;
				E.cx = E.row[E.cy].size;
			}
			break;
		case ARROW_RIGHT:
			if (row && E.cx < row->size){
				E.cx++;
			} else if ( row && E.cx == row->size ){
				E.cy++;
				E.cx = 0;
			}
			break;
		case ARROW_UP:
			if (E.cy != 0) {
				E.cy--;
			}
			break;
		case ARROW_DOWN:
			if ( E.cy < E.numrows){
				E.cy++;
			}
			break;
	}

	row = (E.cy >= E.numrows)?NULL : &E.row[E.cy];
	int rowlen = row ? row->size : 0;
	if( E.cx > rowlen){
		E.cx = rowlen;
	}
}

void editorProcessKeypress(){
	int c = editorReadKey();

	switch(c){
		case CTRL_KEY('q'):
			// Clear the screen on exit
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;
		
		case HOME_KEY:
			E.cx = 0;
			break;

		case END_KEY:
			E.cx = E.screencols - 1;
			break;

		case PAGE_UP:
		case PAGE_DOWN:
			{
				int times = E.screenrows;
				while(times--){
					editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
				}
			}
			break;
		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
			editorMoveCursor(c);
			break;
	}
}


/*** init ***/

void initEditor(){
	E.cx = 0;
	E.cy = 0;
	E.rowoff = 0;
	E.coloff = 0;
	E.numrows = 0;
	E.row = NULL;

	if (getWindowSize(&E.screenrows, &E.screencols) == -1 ) die ("getWindowSize");
}

int main(int argc, char *argv[]) {
	// Turn the terminal to Raw mode from Canonical Mode
	enableRawMode();
	initEditor();

	if (argc >= 2){
		editorOpen(argv[1]);
	}

	// Read 1 byte from the standard input into 'c' until no bytes left to read or q key is entered
	while (1){
		editorRefreshScreen();
		editorProcessKeypress();
	}
	return 0;
}
