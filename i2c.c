
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "i2cctl.h"

int main(char argc, char** argv)
{	
	if (argc > 1 && !strcmp(argv[1], "lock"))   { i2c_lock();		return 0; }
	if (argc > 1 && !strcmp(argv[1], "unlock")) { i2c_release();	return 0; }
	
	if (argc < 4)
	{	printf("Usage: i2c {addr} {reg} {length_bytes} {R/W} [{value}]\n");
		printf("Give all numbers in hexadecimal.\n");
		return -1;
	}

	char** end;
	unsigned long value;
	int addr = strtol(argv[1], end, 16);
	int reg  = strtol(argv[2], end, 16);
	int len  = strtol(argv[3], end, 16);
	
	switch (argv[4][0])
	{	case 'r':
		case 'R':
		value = readreg(addr, reg, len);
		printf("%X\n", value);
		return value;
		break;
		
		case 'w':
		case 'W':
		if (argc > 4) value = strtol(argv[5], end, 16);
		else value = 0;
		int res = writereg(addr, reg, value, len);
		if (!res) printf("Success!\n");
		return res;
		break;
		
		default:
		printf("Unknown R/W value %s", argv[4]);
		return -1;
	}
	
}
