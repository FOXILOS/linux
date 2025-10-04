#include <string.h>
#include <readline/readline.h>
#include <stdbool.h>
#include <stdlib.h>
void debug(char *line){
	printf("%s\n", line);
}
int main(){
	char *input;

	while(true){
	input = readline("$ ");

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
return 0;
}

