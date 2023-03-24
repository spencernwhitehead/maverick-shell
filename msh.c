/*

	Name: Spencer Whitehead
	ID: 1001837401
	
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#define WHITESPACE " \t\n"		// We want to split our command line up into tokens
								// so we need to define what delimits our tokens.
								// In this case  white space
								// will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255	// The maximum command-line size

//#define MAX_NUM_ARGUMENTS 5	// Mav shell only supports four arguments

#define MAX_NUM_ARGUMENTS 11	// requirement 7 says the shell needs to take 10 command line
								// parameters along with the command itelf,
								// so this should be 11 right?

#define MAX_NUM_PREV_PIDS 20	// defining the array sizes here
#define MAX_NUM_PREV_CMDS 15	// because it felt weird to hard code them further down

int main() {

	char * command_string = (char*) malloc( MAX_COMMAND_SIZE );
	
	// arrays for keeping track of previous pids and commands outside of the shell loop
	int prev_pids[MAX_NUM_PREV_PIDS];
	char *prev_cmds[MAX_NUM_PREV_CMDS];
	
	// indexes for the arrays start at -1 because they get incremented before the first use
	int prev_pids_index = -1;
	int prev_cmds_index = -1;
	
	// everyone's favorite for loop iterator!
	int i;
	
	// used as a boolean to check whether or not a command is being recycled,
	// or if the shell should grab a new one from stdin
	int reuse_cmd = 0;

	while( 1 ) {
		if( !reuse_cmd ) {
			// Print out the msh prompt
			printf ("msh> ");

			// Read the command from the commandline.  The
			// maximum command that will be read is MAX_COMMAND_SIZE
			// This while command will wait here until the user
			// inputs something since fgets returns NULL when there
			// is no input
			while( !fgets (command_string, MAX_COMMAND_SIZE, stdin) );
		}
		// set the reuse cmd back to false so the shell will get input from user next time
		else {
			reuse_cmd = 0;
		}

		/* Parse input */
		char *token[MAX_NUM_ARGUMENTS];

		int   token_count = 0;                                 
		
		// Pointer to point to the token
		// parsed by strsep
		char *argument_ptr;                                         
		
		char *working_string = strdup( command_string );                

		// we are going to move the working_string pointer so
		// keep track of its original value so we can deallocate
		// the correct amount at the end
		char *head_ptr = working_string;

		// Tokenize the input strings with whitespace used as the delimiter
		while ( ( (argument_ptr = strsep(&working_string, WHITESPACE ) ) != NULL) && 
				(token_count<MAX_NUM_ARGUMENTS)) {
			token[token_count] = strndup( argument_ptr, MAX_COMMAND_SIZE );
			if( strlen( token[token_count] ) == 0 ) {
				token[token_count] = NULL;
			}
			token_count++;
		}
		
		// if the command is not just an empty return statement,
		// store it in the previous commands array
		if( token[0] != NULL ) {
			
			// if the previous commands array is full, 
			// bump all the values to the left to fit the new one
			if ( prev_cmds_index == MAX_NUM_PREV_CMDS - 1 ) {
				free( prev_cmds[0] );
				for( i = 0; i < MAX_NUM_PREV_CMDS - 1; i++ ) {
					prev_cmds[i] = prev_cmds[i + 1];
				}
			}
			// otherwise array is not full
			// increment index for the new cmd
			else {
				prev_cmds_index++;
			}
			
			// actual storing of cmd in array
			prev_cmds[prev_cmds_index] = (char*) malloc( sizeof(command_string) );
			strcpy( prev_cmds[prev_cmds_index], command_string );
		}

		// due to the possibility of a continue, figured it'd be better to free this here 
		// since it's not used further down anyway
		free( head_ptr );
		
		// checks for blank line, and if so restarts the loop to get more input
		if( token[0] == NULL ) {
			continue;
		}
		
		// exits the program if user types either of the magic words
		else if( strcmp( token[0], "quit" ) == 0 || strcmp( token[0], "exit" ) == 0 ) {
			return 0;
		}
		
		// handling for cd cmd with chdir, also prints new dir
		// hoping that there won't be a directory longer than 256 chars
		else if( strcmp( token[0], "cd" ) == 0 ) {
			chdir( token[1] );
			char cwd[MAX_COMMAND_SIZE];
			printf("directory changed to: %s\n", getcwd( cwd, sizeof( cwd ) ) );
		}
		
		// prints through pid array on listpids cmd
		else if( strcmp( token[0], "listpids" ) == 0 ) {
			for( i = 0; i < prev_pids_index + 1; i++ ) {
				printf("%d: %d\n", i, prev_pids[i]);
			}
		}
		
		// prints through cmd array on history cmd
		else if( strcmp( token[0], "history" ) == 0 ) {
			for( i = 0; i < prev_cmds_index + 1; i++ ) {
				printf( "%d: %s", i, prev_cmds[i] );
			}
		}
		
		// calls previous command on !n
		else if( token[0][0] == '!' ) {
			// num_str pulls the n out of !n
			char *num_str = token[0] + sizeof( char );
			// converts n to an int
			int call_cmd = atoi( num_str );
			// shuns the user if n is out of range of the cmds array
			if( call_cmd > prev_cmds_index || call_cmd < 0 ) {
				printf("command not in history\n");
			}
			// if in range, puts the nth cmd back into command_string so it can be executed again
			// also activates the reuse_cmd flag to skip the stdin stuff
			else {
				strcpy( command_string, prev_cmds[call_cmd] );
				reuse_cmd = 1;
			}
		}
		
		// any other input to the shell is either not an unknown command,
		// or a command to run with exec
		else {
			// split time!
			pid_t pid = fork( );
			if( pid == 0 ) {
				// try to execute the input as a command as the child process
				int ret = execvp( token[0], &token[0] );  
				if( ret == -1 ) {
					// assuming here that if execvp returns an error,
					// the command just doesn't exist
					// so this should fulfill requirement 2
					printf("%s: command not found\n", token[0]);
				}
			}
			else {
				if( pid != 0 ) {
					// add pid to the previous pids array
					// if array is full, bump everything over to put it at the end
					if ( prev_pids_index == MAX_NUM_PREV_PIDS - 1 ) {
						for( i = 0; i < MAX_NUM_PREV_PIDS - 1; i++ ) {
							prev_pids[i] = prev_pids[i + 1];
						}
					}
					// otherwise array is not full
					// increment index for new pid
					else {
						prev_pids_index++;
					}
					
					prev_pids[prev_pids_index] = pid;
				}
				
				// after storing the pid
				// wait until child process is done doing whatever it's doing
				int status;
				wait( & status );
			}
		}	
	}
	return 0;
}
