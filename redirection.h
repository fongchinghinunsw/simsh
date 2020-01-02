// return True if 'words' contains '<' or '>'.
int is_redirection(char **words);


// check if the command is an input redirection
int is_input_redirection(char **words);


// check if the command is an output redirection in write mode
int is_output_write_redirection(char **words);


// check if the command is an output redirection in append mode
// return True if 'words' contains two consecutive '>'.
int is_output_append_redirection(char **words);


// return True if '<' and '>' appears in the valid position.
// '<' must be the first word, while '>' can appear in the
// second or third last word.
// if '>' appears in the third last word than '>' must also
// appears in the second last word.
int is_valid_redirection_position(char **words);


// return True if the command involved in redirection is not
// a builtin command.
int is_valid_redirection_program(char **words);


// this function is used if the command contains only input redirection.
void input_redirection(char **tokens, char **path, char **environ);


// this function is used if the command contains only output redirection
// in write mode.
void output_write_redirection(char **tokens, char **path, char **environ);


// this function is used if the command contains only output redirection
// in append mode.
void output_append_redirection(char **tokens, char **path, char **environ);


// this function is used if the command contains one input redirection
// and one output redirection.
void input_output_redirection(char **tokens, char **path, char **environ);


// this function is used to return the arguments array for an output
// redirection command.
char **get_args_for_output_redirection(char **tokens);
