#include <termios.h>
#include <unistd.h>

void enableRawMode(){
	struct termios raw;
	
	// Get the Terminal Data in to raw;
	tcgetattr(STDIN_FILENO, &raw);

	// Turn off Echo
	raw.c_lflag &= ~(ECHO);
	
	// Set the Terminal Data
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main() {
	// Turn the terminal to Raw mode from Canonical Mode
	enableRawMode();
	// Variable to get the input
	char c;
	// Read 1 byte from the standard input into 'c' until no bytes left to read or q key is entered
	while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q');
	return 0;
}
