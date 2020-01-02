// Returns true if the string represents an integer.
int is_integer(char *string);


// Returns true if the command includes piping.
int is_pipes(char **tokens);


// Returns # word in an array of words.
int count_nwords(char **words);


// Prints a program indicating too many arguments for the program.
void print_too_many_arguments(char *program);


// Returns true if the command is a builtin command.
int is_builtin_command(char *command);

// Returns true if str starts with pre.
bool startsWith(const char *pre, const char *str);
