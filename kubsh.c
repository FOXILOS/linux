#include <readline/history.h>
#define HISTORY_FILE ".kubsh_history"

#include <signal.h>
#include <string.h>
#include <readline/readline.h>
#include <stdbool.h>
#include <stdlib.h>

sig_atomic_t signal_received = 0;

void debug(char *line){
	printf("%s\n", line);
}

void sig_handler(int signum){
	signal_received = signum;
	printf("Configuration reload");
}

int main(){
	signal(SIGINT, sig_handler);
	read_history(HISTORY_FILE);
	char *input;


	while(true){
	input = readline("$ ");

	if(signal_received){
		signal_received = 0;
		continue;
	}

	if(input == NULL){
		break;
	}
	add_history(input);

	if(input == NULL || *input == '\0'){
		free(input);
		break;
	}

	if(!strcmp(input, "\\q")){
		break;
	} else if (strncmp(input, "debug ", 5) == 0){
	  debug(input);
	} else {
	  printf("%s: command not found\n", input);
	}
	free(input);
	}
write_history(HISTORY_FILE);
return 0;
}

