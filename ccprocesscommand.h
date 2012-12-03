#include <stdio.h>

void split(char *command, char **result);

void split(char *command, char **result){
	sscanf(command,"%s %s %s",*result,*(result+1),*(result+2));
}
