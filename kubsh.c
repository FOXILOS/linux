#include <stdio.h>
#include <string.h>

int main(){
char input[100];
while(1){
	fgets(input,100,stdin);
	input[strlen(input)- 1] = '\0';
	if(strcmp(input, "\\q") == 0){
		break;
	}
	printf("%s: command not found\n", input);
	if(strstr(input, "debug \"") == input) {
		char *start = strchr(input, '\"');
		char *end = strchr(start + 1, '\"');
		if(start != NULL && end != NULL) {
			printf("DEBUG: ");
			for(char *p = start + 1; p < end; p++){
				putchar(*p);
			}
			printf("\n");
		}
	}else{
		printf("%s: command not found\n", input);
	}
}
 return 0;
}

