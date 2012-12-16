#include <stdio.h>

int main(int argc, char **argv){
	char line[40] = "";
	read(0,line,40);
	printf("%s\n",line);
	return 0;
}
