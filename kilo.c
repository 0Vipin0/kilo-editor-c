#include <unistd.h>

int main() {
	// Variable to get the input
	char c;
	// Read 1 byte from the standard input into 'c' until no bytes left to read or q key is entered
	while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q');
	return 0;
}
