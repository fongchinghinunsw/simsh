// Get the absolute path to the .cowrie_history file.
char *get_history_path();


// Prints lines in history based on the argument.
void print_history(char *asciiNumber);


// Append the command line to the .cowrie_history file.
void write_to_history(char **command);


// Returns number of lines in .cowrie_history file.
int get_nlines();
