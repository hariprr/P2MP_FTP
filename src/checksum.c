#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<string.h>

int16_t add_16_bit(int16_t num1, int16_t num2){
	printf("Adding %02x and %02x\n",num1 & 0xFFFF ,num2 & 0xFFFF);
	if ( num1 == 0 )
		return num2;
	if ( num2 == 0 )
		return num1;
	int16_t carry = num1 & num2;
	int16_t sum = num1 ^ num2;
	sum = add_16_bit(sum,carry << 1);
	return sum;
}

int main(int argc, char *argv[]){
/*	int16_t num1=0b1111;
	int16_t num2=0b0001;
	printf("Sum is %d\n", add_16_bit(num1,num2));	
*/
	

	char buf[1500]="mynameisttusharnd this is ample data to trafansefer ? what is my checksum?";
	uint16_t *checksum_ptr,checksum_value=0,*end_ptr,current_value;
	checksum_ptr=(int16_t *)&argv[1][0];
	end_ptr=(uint16_t *)&argv[1][strlen(argv[1])];
	while ( checksum_ptr != end_ptr){
		printf("Adding %02x and %02x\n",checksum_value,*checksum_ptr);
		checksum_value = add_16_bit(checksum_value,*checksum_ptr);
		printf("Checksum is %d\n",checksum_value);
		checksum_ptr +=1;
	}
	printf("Checksum is %d\n",checksum_value);
	return 0;		
}
