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

}

 return 0;
}
