#include <stdio.h>
#include <math.h>

int mk_bmp( const int width, 
            const int height, 
            const int pxsize, 
            const unsigned char* rdat, 
            const unsigned char* gdat, 
            const unsigned char* bdat, 
            const char* fname
          );

int read_bmp(   int* width,
                int* height, 
                unsigned char* rdat, 
                unsigned char* gdat, 
                unsigned char* bdat, 
                const char* fname
            );


