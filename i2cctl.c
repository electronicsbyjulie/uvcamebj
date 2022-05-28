#define _XOPEN_SOURCE

#include <unistd.h>				// for I2C port
#include <fcntl.h>				// for I2C port
#include <sys/ioctl.h>			// for I2C port
#include <linux/i2c-dev.h>		// for I2C port
// #include <linux/i2c.h>          // for I2C port
#define I2C_M_STOP		0x8000

struct i2c_msg {
  __u16 addr;
  __u16 flags;
#define I2C_M_TEN		0x0010
#define I2C_M_RD		0x0001
#define I2C_M_NOSTART		0x4000
#define I2C_M_REV_DIR_ADDR	0x2000
#define I2C_M_IGNORE_NAK	0x1000
#define I2C_M_NO_RD_ACK		0x0800
#define I2C_M_RECV_LEN		0x0400
  __u16 len;
  __u8 * buf;
};  

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "i2cctl.h"

// for shared memory
#include <sys/ipc.h> 
#include <sys/shm.h> 

#define IN  0
#define OUT 1
 
#define LOW  0
#define HIGH 1

#define max_try 300

int file_i2c;
unsigned char buffer[60] = {0};
int i2cinitd = 0;

key_t keypr;
int shmidpr, *prdat = 0;


#ifndef getMicrotime
long getMicrotime()
{   struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}
#endif


int i2cinit(int addr)
{
	i2cinitd++;
	if (i2cinitd > 1) return 0;

	keypr = ftok("/tmp/shm",1);
	shmidpr  = shmget(keypr, 16*sizeof(int), 0666 | IPC_CREAT);
	prdat = (int*)shmat(shmidpr, (void*)0, 0);
	
	i2c_release();
	
	//----- OPEN THE I2C BUS -----
	char *filename = (char*)"/dev/i2c-1";
	if ((file_i2c = open(filename, O_RDWR)) < 0)
	{
		//ERROR HANDLING: you can check errno to see what went wrong
		printf("Failed to open the i2c bus");
		return 1;
	}
	
	if (ioctl(file_i2c, I2C_SLAVE, addr) < 0)
	{
		// printf("i2cinit: Failed to acquire bus access and/or talk to slave.\n");
		//ERROR HANDLING; you can check errno to see what went wrong
		return 1;
	}
    		
    return 0;
}

int i2cinit4041()
{	int r1, r2;
	r1 = i2cinit(0x40);
	r2 = i2cinit(0x41);
	
	return r1 ? r1 : r2;
}

int i2c_check()
{	if (!prdat) i2cinit4041();
	if (!prdat[0]) return 1;
	if (prdat[0] == getpid()) return 1;
	char buf[256];
	sprintf(buf, "ps -ef | grep %i | grep -v grep", prdat[0]);
	FILE *pf;
	if (pf = popen(buf, "r"))
	{	buf[0] = 0;
		fgets(buf, 255, pf);
		printf("%s\n", buf);
		fclose(pf);
		if (buf[0]) return 0;
		prdat[0] = 0;
		return 1;
	}
}

void i2c_lock()
{	if (!prdat) i2cinit4041();
	prdat[0] = getpid();
}

void i2c_release()
{	if (!prdat) i2cinit4041();
	if (i2c_check()) prdat[0] = 0;
}

static int
GPIOExport(int pin)
{
#define BUFFER_MAX 3
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;
 
	fd = open("/sys/class/gpio/export", O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open export for writing!\n");
		return(-1);
	}
 
	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return(0);
}
 
static int
GPIOUnexport(int pin)
{
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;
 
	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open unexport for writing!\n");
		return(-1);
	}
 
	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return(0);
}
 
static int
GPIODirection(int pin, int dir)
{
	static const char s_directions_str[]  = "in\0out";
 
#define DIRECTION_MAX 35
	char path[DIRECTION_MAX];
	int fd;
 
	snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio direction %d for writing!\n", pin);
		return(-1);
	}
 
	if (-1 == write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3)) {
		fprintf(stderr, "Failed to set direction!\n");
		return(-1);
	}
 
	close(fd);
	return(0);
}
 
static int
GPIORead(int pin)
{
#define VALUE_MAX 30
	char path[VALUE_MAX];
	char value_str[3];
	int fd;
 
	snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, O_RDONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio value for reading!\n");
		return(-1);
	}
 
	if (-1 == read(fd, value_str, 3)) {
		fprintf(stderr, "Failed to read value!\n");
		return(-1);
	}
 
	close(fd);
 
	return(atoi(value_str));
}
 
static int
GPIOWrite(int pin, int value)
{
	static const char s_values_str[] = "01";
 
	char path[VALUE_MAX];
	int fd;
 
	snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio value for writing!\n");
		return(-1);
	}
 
	if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)) {
		fprintf(stderr, "Failed to write value!\n");
		return(-1);
	}
 
	close(fd);
	return(0);
}


int writereg(unsigned int addr, unsigned int reg, unsigned int value, unsigned char length)
{
	while (!i2c_check()) usleep(100000);

	//----- OPEN THE I2C BUS -----
	char *filename = (char*)"/dev/i2c-1";
	if ((file_i2c = open(filename, O_RDWR)) < 0)
	{
		//ERROR HANDLING: you can check errno to see what went wrong
		printf("Failed to open the i2c bus");
		return -1;
	}
	
	if (ioctl(file_i2c, I2C_SLAVE, addr) < 0)
	{
		printf("writereg: Failed to acquire bus access and/or talk to slave.\n");
		//ERROR HANDLING; you can check errno to see what went wrong
		return -1;
	}


	// write 8 LSBs to register 6 and 4 MSBs to register 7
	buffer[0] = reg & 0xff;
	buffer[1] = (length    ) ? (value 		 & 0xff) : 0;
	buffer[2] = (length > 1) ? ((value >> 8) & 0xff) : 0;

	length = 2;			//<<< Number of bytes to write
	if (write(file_i2c, buffer, length) != length)		//write() returns the number of bytes actually written, if it doesn't match then an error occurred (e.g. no response from the device)
	{
		/* ERROR HANDLING: i2c transaction failed */
		printf("Failed to write to the i2c bus errno %d.\n", errno);
	}

    return 0;
}

unsigned long readreg(unsigned int addr, unsigned int reg, unsigned char length)
{
	while (!i2c_check()) usleep(100000);
	
	//----- OPEN THE I2C BUS -----
	char *filename = (char*)"/dev/i2c-1";
	if ((file_i2c = open(filename, O_RDWR)) < 0)
	{
		//ERROR HANDLING: you can check errno to see what went wrong
		printf("Failed to open the i2c bus");
		return -1;
	}
	
	if (ioctl(file_i2c, I2C_SLAVE, addr) < 0)
	{
		printf("readreg: Failed to acquire bus access and/or talk to slave.\n");
		//ERROR HANDLING; you can check errno to see what went wrong
		return -1;
	}


	//----- SET REGISTER -----
	
	int i;

    buffer[0] = reg & 0xff;
    //length = 1;			//<<< Number of bytes to write
    if (write(file_i2c, buffer, length) != length)		//write() returns the number of bytes actually written, if it doesn't match then an error occurred (e.g. no response from the device)
    {
	    /* ERROR HANDLING: i2c transaction failed */
	    printf("Failed to write register number %d to the i2c bus.\n", buffer[0]);
            //buffer = g_strerror(errno);
            printf("%d",errno);
        	printf("\n\n");
        	return 1;
    }


    //----- READ BYTES -----
    unsigned char h, l;
    //length = 1;			//<<< Number of bytes to read
    if (read(file_i2c, buffer, length) != length)		//read() returns the number of bytes actually read, if it doesn't match then an error occurred (e.g. no response from the device)
    {
	    //ERROR HANDLING: i2c transaction failed
	    printf("Failed to read from the i2c bus.\n");
            //buffer = g_strerror(errno);
            printf("%d",errno);
        	printf("\n\n");
        	return 1;
    }
    else
    {	unsigned long retval = 0;
	    for (i=0; i < length; i++)
	    {	retval += buffer[i] << (8*i);
	    }
	    return retval;
    }
    
    return -1;
}



unsigned long readreg16(int l_file_i2c, unsigned int reg)
{
    unsigned char l_buffer[2];
    
	struct i2c_msg imsgs[2];
	struct i2c_rdwr_ioctl_data msgset;
	msgset.nmsgs = 2;
	msgset.msgs = imsgs;
	
	l_buffer[0] = reg >> 8;
	l_buffer[1] = reg & 0xFF;
	
	imsgs[0].addr = 0x33;
	imsgs[0].flags = 0;
	imsgs[0].len = 2;
	imsgs[0].buf = l_buffer;
	
	imsgs[1].addr = 0x33;
	imsgs[1].flags = I2C_M_RD | I2C_M_STOP;
	imsgs[1].len = 2;
	imsgs[1].buf = l_buffer;
	
	
	int result = ioctl(l_file_i2c, I2C_RDWR, &msgset);
	
	if (result < 0)
    {
        printf("ioctl error: %s\n", strerror(errno));
        return -1;
    }
    
    return l_buffer[1] + 0x100*l_buffer[0];
}


unsigned long writereg16(int l_file_i2c, unsigned int reg, unsigned int value)
{
	//----- SET REGISTER -----
	
	int i, l, length;

    buffer[0] = (reg   & 0xff00) >> 8;
    buffer[1] =  reg   & 0x00ff;
    buffer[2] = (value & 0xff00) >> 8;
    buffer[3] =  value & 0x00ff;
    
    length = 4;			//<<< Number of bytes to write
    if ((l=write(l_file_i2c, buffer, length)) != length)		//write() returns the number of bytes actually written, if it doesn't match then an error occurred (e.g. no response from the device)
    {
	    /* ERROR HANDLING: i2c transaction failed */
	    printf("Failed to write register number %d to the i2c bus.\n", reg);
            //buffer = g_strerror(errno);
            printf("%d len %d",errno, l);
        	printf("\n\n");
        	return 1;
    }
    
    return 0;
}



int i2copen(int address)
{
    int file_i2c;

	//----- OPEN THE I2C BUS -----
	char *filename = (char*)"/dev/i2c-1";
	if ((file_i2c = open(filename, O_RDWR)) < 0)
	{
		//ERROR HANDLING: you can check errno to see what went wrong
		printf("Failed to open the i2c bus");
		return -1;
	}
	
	int addr = address;          
	if (ioctl(file_i2c, I2C_SLAVE, addr) < 0)
	{
		printf("i2copen: Failed to acquire bus access and/or talk to slave.\n");
		//ERROR HANDLING; you can check errno to see what went wrong
		return -1;
	}

    return file_i2c;
}

int i2c_write(unsigned char* buffer, unsigned int length, int i2cfile)
{
	while (!i2c_check()) usleep(100000);
    //delay(20);
    int written = 0;
    for (int iter = 0; iter <= max_try; iter++)
    {
        if (!(written = write(i2cfile, buffer, length)))		//write() returns the number of bytes actually written, if it doesn't match then an error occurred (e.g. no response from the device)
	    {
		    if (iter >= max_try)
		    {
		    // printf("Failed to write register number %d to the i2c bus.\n", buffer[0]);
	            //buffer = g_strerror(errno);
	            // printf("%d",errno);
            	// printf("\n\n");
            	return written;
        	}
	    }
	    else return written;
    }
    return written;
}

int i2c_read(unsigned char* buffer, unsigned int regno, unsigned int length, int file_i2c)
{
	int written;

    //----- SET REGISTER -----
    buffer[0] = regno;
    i2c_write(buffer, 1, file_i2c);

    //----- READ BYTES -----
    if (!(written = read(file_i2c, buffer, length)))		//read() returns the number of bytes actually read, if it doesn't match then an error occurred (e.g. no response from the device)
    {
	    //ERROR HANDLING: i2c transaction failed
	    // printf("Failed to read register %d from the i2c bus.\n", regno);
            //buffer = g_strerror(errno);
            // printf("%d",errno);
        	// printf("\n\n");
        	return written;
    }
    else
    {
	    //printf("Data read: %s\n", buffer);
	    /*for (int j=0; j<length; j++)
	    {	
		    printf("%02X", buffer[j]);
	    }
	    printf(" ");*/
	    
	    //if ((i & 0x3) == 0x3) printf(" ");
	    //if ((i & 0xf) == 0xf) printf("\n");
    }
    

    //delay(10);
    
    return written;
}



