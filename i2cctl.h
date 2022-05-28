
#ifndef _I2C_CTL_
#define _I2C_CTL_

int 			i2cinit(int addr);
int 			writereg(unsigned int addr, unsigned int reg, unsigned int value, unsigned char length);
unsigned long 	readreg(unsigned int addr, unsigned int reg, unsigned char length);
unsigned long   readreg16(int file_i2c, unsigned int reg);
unsigned long   writereg16(int l_file_i2c, unsigned int reg, unsigned int value);

int i2copen(int address);
int i2c_write(unsigned char* buffer, unsigned int length, int i2cfile);
int i2c_read(unsigned char* buffer, unsigned int regno, unsigned int length, int file_i2c);


void i2c_lock();
void i2c_release();

static int	GPIOExport(int pin);
static int	GPIOUnexport(int pin);
static int	GPIODirection(int pin, int dir);
static int	GPIORead(int pin);
static int	GPIOWrite(int pin, int value);

extern unsigned char buffer[60];
long getMicrotime();

#endif

