#include <stdio.h>

int main(){
	int a = 10;
	if(a < 20){
		printf("a is less than 20\n");
	} else if(a < 10){
		printf("a is less than 10\n");
	} else {
		printf("last branch\n");
	}

	int b = 10;
	switch(b){
		case 20:
			printf("b is 20\n");
			break;
		case 10:
			printf("b is 10\n");
			break;
		default:
			printf("lase case\n");
	}

	return 0;
}
