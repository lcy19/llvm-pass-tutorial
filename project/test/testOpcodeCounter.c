#include <stdio.h>

int func(int a,int b){ // a*b
	int sum = 0;
    	for(int i = 0; i < a; i++) {
    		sum += b;
    	}
    	return sum;
}

int main(){
	int a = 6;
	int b = 7;
	printf("%d * %d = %d\n", a, b, func(a, b));

	return 0;
}
