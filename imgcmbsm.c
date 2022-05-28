#define _XOPEN_SOURCE

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <jpeglib.h>    
#include <jerror.h>
#include <png.h>

// for shared memory
#include <sys/ipc.h> 
#include <sys/shm.h> 

#include "mkbmp.h"
#include "dispftns.h"

// Thermal camera selection (default without any defines = AMG8833)
#define _MLX90640

// Added lightness for thermal overlay
#define _thermlite 255

#define _USE_5D_THERMCMB 0

// RGB factor for 5D thermal-optical alignment
// TODO: make this depend on spacing constants also make this a one-time global for performance reasons
#define rgbf 0.0007

// darkness compensation for hues. 
// Increase if night shots are too infrared; 
// decrease if monochrome illumination looks washed out.
#define iauc 0.01

#ifdef _MLX90640

#define _THERM_W 32
#define _THERM_H 24

// Thermcam offset to align to NoIR - multiply these by wid, hei
float therm_off_x =     0.215;
float therm_off_y =     0.15;

// Thermcam is mounted crooked because I'm under the weather and drunk
float therm_rot_rad =   2.0 * 3.1415926535897932384626 / 180;

// NoIR size per thermcam 16x16-pixel block - multiply these by wid, hei
#define therm_sz_x 0.0213
#define therm_sz_y 0.0285

// Half-size for new thermal squares to be drawn
#define sq_hsz 4

#else

#define _THERM_W 8
#define _THERM_H 8

// Thermcam offset to align to NoIR - multiply these by wid, hei
#define therm_off_x 0.17
#define therm_off_y 0.05

// NoIR size per thermcam 16x16-pixel block - multiply these by wid, hei
#define therm_sz_x 0.10
#define therm_sz_y 0.12

// Half-size for new thermal squares to be drawn
#define sq_hsz 10

#endif

// Eye offset to align to NoIR - multiply these by wid, hei
#define eye_off_x 0.02
#define eye_off_y -.125

// NoIR size per eye pixel - multiply these by wid, hei
#define eye_sz_x 0.195
#define eye_sz_y 0.25

#define line_spacing 81
#define line_width 8

// Mappings
#define _ugb 0x659
#define _ubg 0x695
#define _gbu 0x596
#define _gub 0x569
#define _bug 0x965
#define _bee 0xbee
#define _bgu 0x956
#define _mon 0x111
#define _raw 0xbadbeef


int therm_mode = _THM_HUE;

typedef struct
{
    unsigned int x;
    unsigned int y;
    uint8_t r;
    uint8_t g;
    uint8_t b;
}
pixel_5d;


typedef struct
{	unsigned int idx[4];
	float wt[4];
}
wghidx;


float distance_between_pix5(pixel_5d a, pixel_5d b)
{	
	float r, d;
	d = a.x - b.x; r  = d*d;
	d = a.y - b.y; r += d*d;
	
	d = rgbf*(a.r - b.r); r += d*d;
	d = rgbf*(a.g - b.g); r += d*d;
	d = rgbf*(a.b - b.b); r += d*d;
		  
	return pow(r, 0.5);
}

pixel_5d thermspot[_THERM_W*_THERM_H];
unsigned char thermr[_THERM_W*_THERM_H], thermg[_THERM_W*_THERM_H], thermb[_THERM_W*_THERM_H];
int wid, hei, wid2, hei2;
float thmult[780];

int nearest_thermspot_5d(pixel_5d target)
{	
	int x, y, i, j, k, l;
	float r1, r2;
	
	// First, narrow it down by Y
	r1 = 999999;
	j = -1;
	for (y=0; y<_THERM_H; y++)
	{	i = _THERM_W*y;
		r2 = abs(thermspot[i].y - target.y);
		if (r2 < r1) { r1 = r2; j = y; }
	}
	
	// Then, narrow it down by X
	r1 = 999999;
	k = -1;
	for (x=0; x<_THERM_W; x++)
	{	i = _THERM_W*j + x;
		r2 = abs(thermspot[i].x - target.x);
		if (r2 < r1) { r1 = r2; k = x; }
	}
	
	// Now scan
	r1 = 999999;
	l = k + _THERM_W*j;					// default in case we don't find
	// return l;
	for (y = j-2; y <= j+2; y++)
	{	if (y < 0) y = 0;
		if (y >= _THERM_H) break;
		for (x = k-2; x <= k+2; x++)
		{	if (x<0) x = 0;
			if (x>=_THERM_W) continue;
			
			i = x + _THERM_W*y;
			
			r2 = distance_between_pix5(target, thermspot[i]);
			if (r2 < r1) { r1 = r2; l = i; }
		}
	}
	
	return l;	
}

wghidx blur_thermspot(pixel_5d target)
{	int x, y, i, j, k, l;
	wghidx retval;
	
#if _USE_5D_THERMCMB
	int cx0, cx1, cy0, cy1;
	int htx, hty;
	
	htx = (wid*therm_sz_x);
	hty = (hei*therm_sz_y);
	
	cx0 = target.x - htx;
	cx1 = target.x + htx;
	cy0 = target.y - hty;
	cy1 = target.y + hty;
	
	int q1, q2, q3, q4;
	
	target.x = cx0;
	target.y = cy0;
	retval.idx[0] = q1 = nearest_thermspot_5d(target);
	float r1 = distance_between_pix5(target, thermspot[q1]);
	target.x = cx1;
	retval.idx[1] = q2 = nearest_thermspot_5d(target);
	float r2 = distance_between_pix5(target, thermspot[q2]);
	target.y = cy1;
	retval.idx[2] = q3 = nearest_thermspot_5d(target);
	float r3 = distance_between_pix5(target, thermspot[q3]);
	target.y = cy0;
	retval.idx[3] = q4 = nearest_thermspot_5d(target);
	float r4 = distance_between_pix5(target, thermspot[q4]);
	
	r1 = 1.0/(r1+0.001);
	r2 = 1.0/(r2+0.001);
	r3 = 1.0/(r3+0.001);
	r4 = 1.0/(r4+0.001);
	
	float sumr = r1+r2+r3+r4;
	
    retval.wt[0] = r1 / sumr;
    retval.wt[1] = r2 / sumr;
    retval.wt[2] = r3 / sumr;
    retval.wt[3] = r4 / sumr;

	return retval;
	
#else
	
	float r0, r1, r2, r3;	
	float tx, ty;
	
	// Apply the thermcam offset
	tx = target.x - wid*therm_off_x;
	ty = target.y - hei*therm_off_y;
	
	
	// Apply the rotation
	float sr, cr;
	sr = sin(-therm_rot_rad);
	cr = cos(-therm_rot_rad);
	
	tx -= wid2;
	ty -= hei2;
	
	tx = tx * cr - ty * sr;
	ty = ty * cr + tx * sr;

	tx += wid2;
	ty += hei2;
	
	
	// Divide by the therm pixel spacing
	tx /= (wid*therm_sz_x);
	ty /= (hei*therm_sz_y);
	
	
	// Clip the limits
	if (tx < 0) tx = 0;
	if (tx >= _THERM_W) tx = _THERM_W-1;
	if (ty < 0) ty = 0;
	if (ty >= _THERM_H) ty = _THERM_H-1;
	
	
	// Take the floors and set aside the remainders 
	int txi, tyi;
	
	txi = floor(tx);
	tyi = floor(ty);
	
	float dx, dy;
	dx = tx - txi;
	dy = ty - tyi;
	
	
	// TODO: Perform a bicubic resample
	// For now just linear is fine
	retval.idx[0] = txi + _THERM_W*tyi;
	
	retval.idx[1] = (txi < _THERM_W-1)
	              ? retval.idx[0]+1
	              : retval.idx[0];
	              ;
	              
    retval.idx[2] = (tyi < _THERM_H-1)
                  ? retval.idx[0]+_THERM_W
                  : retval.idx[0];
                  ;
                  
    retval.idx[3] = (tyi < _THERM_H-1)
                  ? retval.idx[1]+_THERM_W
                  : retval.idx[1];
                  ;
                  
    // retval.wt[0] = 1; retval.wt[1] = retval.wt[2] = retval.wt[3] = 0; return retval;
	
	r0 = abs(target.r - thermspot[retval.idx[0]].r)
	   + abs(target.g - thermspot[retval.idx[0]].g)
	   + abs(target.b - thermspot[retval.idx[0]].b)
	   ;
	r1 = abs(target.r - thermspot[retval.idx[1]].r)
	   + abs(target.g - thermspot[retval.idx[1]].g)
	   + abs(target.b - thermspot[retval.idx[1]].b)
	   ;
	r2 = abs(target.r - thermspot[retval.idx[2]].r)
	   + abs(target.g - thermspot[retval.idx[2]].g)
	   + abs(target.b - thermspot[retval.idx[2]].b)
	   ;
	r3 = abs(target.r - thermspot[retval.idx[3]].r)
	   + abs(target.g - thermspot[retval.idx[3]].g)
	   + abs(target.b - thermspot[retval.idx[3]].b)
	   ;
	   
	r0 = r0 * rgbf +        dx  *        dy ;
	r1 = r1 * rgbf + (1.0 - dx) *        dy ;
	r2 = r2 * rgbf +        dx  * (1.0 - dy);
	r3 = r3 * rgbf + (1.0 - dx) * (1.0 - dy);
                  
    /*
    retval.wt[0] = (1.0 - dx) * (1.0 - dy);
    retval.wt[1] =        dx  * (1.0 - dy);
    retval.wt[2] = (1.0 - dx) *        dy ;
    retval.wt[3] =        dx  *        dy ;
    */
	
	r1 = 1.0/(r1+0.001);
	r2 = 1.0/(r2+0.001);
	r3 = 1.0/(r3+0.001);
	r0 = 1.0/(r0+0.001);
	
	float sumr = r1+r2+r3+r0;
	
    retval.wt[0] = r0 / sumr;
    retval.wt[1] = r1 / sumr;
    retval.wt[2] = r2 / sumr;
    retval.wt[3] = r3 / sumr;
	
	return retval;
	
#endif
}


/* A coloured pixel. */

typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
}
pixel_t;

/* A picture. */
    
typedef struct
{
    pixel_t *pixels;
    size_t width;
    size_t height;
}
bitmap_t;
    
/* Given "bitmap", this returns the pixel of bitmap at the point 
   ("x", "y"). */

static pixel_t * pixel_at (bitmap_t * bitmap, int x, int y)
{
    return bitmap->pixels + bitmap->width * y + x;
}
    
/* Write "bitmap" to a PNG file specified by "path"; returns 0 on
   success, non-zero on error. */

static int save_png_to_file (bitmap_t *bitmap, const char *path)
{
    FILE * fp;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    size_t x, y;
    png_byte ** row_pointers = NULL;
    /* "status" contains the return value of this function. At first
       it is set to a value which means 'failure'. When the routine
       has finished its work, it is set to a value which means
       'success'. */
    int status = -1;
    /* The following number is set by trial and error only. I cannot
       see where it it is documented in the libpng manual.
    */
    int pixel_size = 3;
    int depth = 8;
    
    fp = fopen (path, "wb");
    if (! fp) {
		printf("fopen failed.\n");
        goto fopen_failed;
    }

	// MingW has a version mismatch; hopefully RPi doesn't.
    png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
		printf("create write struct failed.\n");
        goto png_create_write_struct_failed;
    }
    
    info_ptr = png_create_info_struct (png_ptr);
    if (info_ptr == NULL) {
		printf("create info struct failed.\n");
        goto png_create_info_struct_failed;
    }
    
    /* Set up error handling. */

    if (setjmp (png_jmpbuf (png_ptr))) {
		printf("jmpbuf failed.\n");
        goto png_failure;
    }
    
    /* Set image attributes. */

    png_set_IHDR (png_ptr,
                  info_ptr,
                  bitmap->width,
                  bitmap->height,
                  depth,
                  PNG_COLOR_TYPE_RGB,
                  PNG_INTERLACE_NONE,
                  PNG_COMPRESSION_TYPE_DEFAULT,
                  PNG_FILTER_TYPE_DEFAULT);
    
    /* Initialize rows of PNG. */

    row_pointers = png_malloc (png_ptr, bitmap->height * sizeof (png_byte *));
    for (y = 0; y < bitmap->height; y++) {
        png_byte *row = 
            png_malloc (png_ptr, sizeof (uint8_t) * bitmap->width * pixel_size);
        row_pointers[y] = row;
        for (x = 0; x < bitmap->width; x++) {
            pixel_t * pixel = pixel_at (bitmap, x, y);
            *row++ = pixel->red;
            *row++ = pixel->green;
            *row++ = pixel->blue;
        }
    }
    
    /* Write the image data to "fp". */

    png_init_io (png_ptr, fp);
    png_set_rows (png_ptr, info_ptr, row_pointers);
    png_write_png (png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    /* The routine has successfully written the file, so we set
       "status" to a value which indicates success. */

    status = 0;
    
    for (y = 0; y < bitmap->height; y++) {
        png_free (png_ptr, row_pointers[y]);
    }
    png_free (png_ptr, row_pointers);
    
 png_failure:
 png_create_info_struct_failed:
    png_destroy_write_struct (&png_ptr, &info_ptr);
 png_create_write_struct_failed:
    fclose (fp);
 fopen_failed:
    return status;
}

/* Given "value" and "max", the maximum value which we expect "value"
   to take, this returns an integer between 0 and 255 proportional to
   "value" divided by "max". */

static int pix (int value, int max)
{
    if (value < 0) {
        return 0;
    }
    return (int) (256.0 *((double) (value)/(double) max));
}


unsigned int type;  
unsigned char * rowptr[1];    // pointer to an array
unsigned char * jdata;        // data for the image
struct jpeg_decompress_struct info; //for our jpeg info
struct jpeg_error_mgr err;          //the error handler

unsigned char* LoadJPEG(char* FileName)
//================================
{
  unsigned long x, y;
  unsigned int texture_id;
  unsigned long data_size;     // length of the file
  int channels;               //  3 =>RGB   4 =>RGBA 

  FILE* file = fopen(FileName, "rb");  //open the file

  info.err = jpeg_std_error(& err);     
  jpeg_create_decompress(& info);   //fills info structure

  //if the jpeg file doesn't load
  if(!file) {
     fprintf(stderr, "Error reading JPEG file %s!", FileName);
     return 0;
  }

  jpeg_stdio_src(&info, file);    
  jpeg_read_header(&info, TRUE);   // read jpeg file header

  jpeg_start_decompress(&info);    // decompress the file

  //set width and height
  x = info.output_width;
  y = info.output_height;
  channels = info.num_components;
  //type = GL_RGB;
  //if(channels == 4) type = GL_RGBA;

  data_size = x * y * 3;

  //--------------------------------------------
  // read scanlines one at a time & put bytes 
  //    in jdata[] array. Assumes an RGB image
  //--------------------------------------------
  jdata = (unsigned char *)malloc(data_size);
  while (info.output_scanline < info.output_height) // loop
  {
    // Enable jpeg_read_scanlines() to fill our jdata array
    rowptr[0] = (unsigned char *)jdata +  // secret to method
            3* info.output_width * info.output_scanline; 

    jpeg_read_scanlines(&info, rowptr, 1);
  }
  //---------------------------------------------------

  jpeg_finish_decompress(&info);   //finish decompressing
  
  return jdata;
}


void load_thalign(void)
{   FILE* pf = fopen("/home/pi/thalign", "r");
	if (pf)
	{	char buffer[1024];
		fgets(buffer, 1024, pf);	therm_off_x = atof(buffer);
	    fgets(buffer, 1024, pf);	therm_off_y = atof(buffer);
	    fgets(buffer, 1024, pf);	therm_rot_rad = atof(buffer);
	    fclose(pf);
	}
}
    

int main(char argc, char** argv)
{
	char *in_ugb, *in_therm, *in_eye, *out_comp;
	unsigned char *rdat, *gdat, *bdat;
	unsigned long rtot, gtot, btot;
	unsigned char rmax, gmax, bmax;
	unsigned char rmin, gmin, bmin;
	int del_inp_f = 0;
	
	int mypid = getpid();
    char cmdbuf[256];
    sprintf(cmdbuf, "sudo renice -n +15 %i", mypid);
    system(cmdbuf);
    

	in_ugb = in_therm = in_eye = 0;

#ifdef _MLX90640
	key_t keyt = ftok("/tmp/shm",73);
	int shmidt  = shmget(keyt, 2048*sizeof(int), 0666 | IPC_CREAT);
	int *thermdatr = (int*)shmat(shmidt, (void*)0, 0);
	int *thermdat = malloc(768*sizeof(int));
	
	for (int i=0; i<768; i++) thermdat[i] = thermdatr[i];
	
#else
	// AMG8833
    key_t keyt = ftok("/tmp/shm",71);
    int shmidt  = shmget(keyt, 80*sizeof(int), 0666 | IPC_CREAT);
    int *thermdat = (int*)shmat(shmidt, (void*)0, 0);
#endif
	
	load_thalign();
	
	
	
	int rotcol = 0, rgb = 0, thermfill = 0; //, fire=0;
	int cmapping = _ugb;
	int found = 0;
	int imono = 0;
	int burst = 0;
	for (int i=1; i<(argc-1); i++)
	{	if (!strcmp(argv[i], "-iugb" )) { in_ugb   = argv[i+1]; rgb = 0; found++; }
		if (!strcmp(argv[i], "-irgb" )) { in_ugb   = argv[i+1]; rgb = 1; found++; }
		if (!strcmp(argv[i], "-therm")) { in_therm = "yes"; found++; }
		if (!strcmp(argv[i], "-o"    )) { out_comp = argv[i+1]; 		 }
		
		if (!strcmp(argv[i], "-r"))		  { rotcol = 1; cmapping = _gbu; }
		if (!strcmp(argv[i], "-mono"))    { imono = 1; cmapping = _mon; }
		if (!strcmp(argv[i], "-tf"))	  thermfill = 1;
		if (!strcmp(argv[i], "-fire"))	  therm_mode = _THM_FIRE;
		if (!strcmp(argv[i], "-fevr"))	  therm_mode = _THM_FEVR;
		if (!strcmp(argv[i], "-bleu"))	  therm_mode = _THM_BLEU;
		if (!strcmp(argv[i], "-tiv"))	  { therm_mode = _THM_TIV; rotcol = 1; cmapping = _gbu; }
		if (!strcmp(argv[i], "-dif"))	  del_inp_f = 1;
		if (!strcmp(argv[i], "-ugb"))	  cmapping = _ugb;
		if (!strcmp(argv[i], "-ubg"))	  cmapping = _ubg;
		if (!strcmp(argv[i], "-gbu"))	  cmapping = _gbu;
		if (!strcmp(argv[i], "-gub"))	  cmapping = _gub;
		if (!strcmp(argv[i], "-bug"))	  cmapping = _bug;
		if (!strcmp(argv[i], "-bee"))	  cmapping = _bee;
		if (!strcmp(argv[i], "-bgu"))	  cmapping = _bgu;
		if (!strcmp(argv[i], "-raw"))	  cmapping = _raw;
		
		if (!strcmp(argv[i], "-b"    )) { burst    = atoi(argv[i+1]); 		 }
	}
	
	// if (therm_mode == _THM_FIRE) printf("fire\n");
	
	if (!out_comp)
	{	printf("No output file.\nUsage:\nimgcomb -iugb NoIR_input.jpg -gbu -o output.bmp\n");
		return 3;
	}
	
	/*if (found < 2)
	{	printf("Nothing to process.\n");
		return 1;
	}*/
	
	
	if (in_ugb)
	{	if (!burst) LoadJPEG(in_ugb);
	    else
	    {   // Load the first image so you have width and height.
	        // Allocate a new array and copy the image.1 data there.
	        // Load the rest of the images, adding them to the values
	        // in the new array, BE CAREFUL to clip overflows rather
	        // than letting them corrupt the LSB.
	        char* jdc;
	        int dsize, ultima=-1, penult=0;
	                
	        for (int i=1; i<=burst; i++)
	        {   
	            if (i >= 2)
	            {   // The temp name will be everything up to including the next to last dot,
	                // then i, then the last dot and everything after.
	                char *pos = in_ugb;
	                ultima=-1, penult=0;
	                while (pos = strchr(pos+1,'.'))
	                {   penult = ultima;
	                    ultima = (pos-in_ugb)/sizeof(char);
	                }
	                
	                printf("Penult %d ultima %d\n", penult, ultima);
	                
	                in_ugb[penult] = 0;
	                in_ugb[ultima] = 0;
                
	                char fnbuf[256];
	                sprintf(fnbuf, "%s.%d.jpg", in_ugb, i, &in_ugb[ultima+2]);
	                strcpy(in_ugb, fnbuf);
	                
	                printf("Filename %i is: %s \n", i, in_ugb);
	            }
	            
	        
	            jdata = LoadJPEG(in_ugb);
	            
	            if (i==1)
	            {   wid = info.output_width;
		            hei = info.output_height;
		            
		            jdc = malloc(dsize=wid*hei*3);
		            for (int j=0; j<dsize; j++) jdc[j] = jdata[j];
	            }
	            else
	            {   int tmp;
	                for (int j=0; j<dsize; j++) 
	                {   tmp = jdc[j] + jdata[j];
	                    if (tmp > 255 || tmp < 0) tmp = 255;
	                    jdc[j] = tmp;
	                }
	            }
	        }
	        
	        for (int j=0; j<dsize; j++) jdata[j] = jdc[j];
	    }
	    
		wid = info.output_width;
		hei = info.output_height;
		wid2 = wid/2;
		hei2 = hei/2;
		
		float gcorr = 0.86 / hei2;
		
		rtot = gtot = btot = wid*hei*16;
		rmax = gmax = bmax = 0;
		rmin = gmin = bmin = 255;
		
		
		int perline = wid*3, pixels = wid*hei;
		
		rdat = (unsigned char *)malloc(pixels);
		gdat = (unsigned char *)malloc(pixels);
		bdat = (unsigned char *)malloc(pixels);
		

        
		unsigned long satttl = 1;
		for (unsigned int y=0; y<hei; y++)
		{	int ly = y * perline;
			int by = y * wid;
			float cy2 = pow(abs(y-hei2),2);
			for (unsigned int x=0; x<wid; x++)
			{	int lx = x * 3;
				int bx = by+x;
				
				float r,g,b;
				
				
				r = /* rdat[bx] =*/ jdata[ly+x*3  ];
				g = /* gdat[bx] =*/ jdata[ly+x*3+1];
				b = /* bdat[bx] =*/ jdata[ly+x*3+2];
				
				#if 0
			    // Green correction for dichroic filter.
			    float cr = pow(pow(abs(x-wid2),2) + cy2, 0.5);
			    float cr1 = cr * gcorr;
			    float cr2 = fabs(cos(cr1));
			    // printf("Coordinates %dx%d center radius %f * correction %f = %f, cosine %f.\n", x, y, cr, gcorr, cr1, cr2);
			    if (!isnan(cr2) && cr2>0) 
			    {   // r = 0.3*r + 0.7*r / cr2;
			        g = 0.7*g + 0.3*g / cr2;
			    }
				#endif
				
				if (r>b && g>b && g>r) r = fmin(255, r + 0.2*(r-b));
				//else if (g>r && b>r) r = fmax(0,fmin(255, 1.0-1.5*(b-r)/b));
				
				if (imono)
				{	float a = 0.67 * r + 0.0 * g + 0.33 * b;
					if (a < r) r = a;
					b = g = r;
				}
				else
                {   
                    /*r = 255 * pow(0.00390625*r, 1.21);
                    g = 255 * pow(0.00390625*g, 1.33);
                    b = 255 * pow(0.00390625*b, 1.25);*/
                    
                    // r -= 16; g -= 16; b -= 16;
                    
                    r /= 255; g /= 255; b /= 255;
                    // r /= 238; g /= 238; b /= 238;
                    
                    /*r = pow(r, 1.25);
                    g = pow(g, 0.85);
                    b = pow(b, 0.95);*/
                    
                    // if (isnan(b)) b = 0;
                    
                    /*if (1) // g>r) 
                    {   b += 0.10*pow(g, 2); if (b>1) b=1;
                        r -= 0.50*pow(g, 2); if (r<0) r=0;
                    }*/
                    
                    r *= 255; g *= 255; b *= 255;
				    
			    }
				

                float plum;			    // pixel lum, pronounced "plume"
				float r_l, g_l, b_l;
				
			    // plum = 0.30 * r + 0.56 * g + 0.14 * b;       // normally.
				// plum = 0.33 * g + 0.34 * b + 0.33 * r;          // this works better.
				plum = 0.20 * g + 0.30 * b + 0.50 * r;          // or try this.
				
				r_l = (r - plum) * 0.7;
				g_l = (g - plum) * 0.7;
				b_l = (b - plum) * 0.7;
				
				b_l -= 0.35*r_l;
								
				// Increase green-blue saturation.
				float g_b = g_l - b_l;
				g_l += 0.22 * g_b;
				b_l -= 0.22 * g_b;
				
				r_l *= 0.53;
				g_l *= 1.00;
				b_l *= 1.00;
				
                if (b_l > g_l && r_l > g_l)
                {   float delta = (fmin(r_l, b_l)-g_l);
                    r_l += 0.11*delta;
                    g_l += 0.25*delta;
                    b_l -= 0.81*delta;
                    
                    float invd = 1.0/(1.0+0.02*delta);
                    g_l *= invd;
                }
				
				if (b_l > r_l && b_l > g_l)
				{   float dimgray = pow(.00392*plum, 0.4);
				    float effect = ((b_l-r_l)+(b_l-g_l))/(plum*2);
				    effect = fmin(1,fmax(0,effect));
				    dimgray = effect*dimgray + (1.0-effect);
				    
				    r_l *= dimgray;
				    g_l *= dimgray;
				    b_l *= dimgray;
				}
				
				                
				
				// if (isnan(b_l)) b_l = 0;
				
				#if 0
				
				float hueshift = 0.125;
				
                float rl1 = r_l + hueshift*b_l - hueshift*g_l;
	            float gl1 = g_l + hueshift*r_l - hueshift*b_l;
		        float bl1 = b_l + hueshift*g_l - hueshift*r_l;
	            
		        r_l = rl1; g_l = gl1; b_l = bl1;
				
				#endif
				
				
				// Compensate for UV dimness.
				r_l = ((plum+r_l)*1.259) - plum;
				
				_place:
				switch (cmapping)
				{   
    				case _bug:
					rdat[bx] = plum + b_l;
				    gdat[bx] = plum + r_l;
				    bdat[bx] = plum + g_l;
				    break;
				    
				    case _bee:
				    //if (b_l > 0) plum -= 0.8 * b_l;
				    // if (b_l < 0) b_l = 0.5;
				    //g_l = 0;
				    
				    if (b_l < 0) b_l /= 2;
				    
				    rdat[bx] = floor(fmax(0,fmin(255,     0.6*plum + r_l + g_l        )));
				    gdat[bx] = floor(fmax(0,fmin(255,     1.3*plum + g_l + fmax(b_l,0) - 1.5*fmax(r_l,0))));
				    bdat[bx] = floor(fmax(0,fmin(255,     0.5*plum + r_l + b_l            )));
				    break;
				    
				    case _bgu:
					rdat[bx] = plum + b_l;
				    gdat[bx] = plum + g_l;
				    bdat[bx] = plum + r_l;
				    break;
				    
				    case _ubg:

					r = plum + r_l;
				    g = plum + b_l;
				    b = plum + g_l;
				    
				    if (r < 0) r = 0; if (g < 0) g = 0; if (b < 0) b = 0;
				    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;

				    
					rdat[bx] = r; //plum + r_l;
				    gdat[bx] = g; //plum + b_l;
				    bdat[bx] = b; //plum + g_l;
				    
				    /*if (rdat[bx] > gdat[bx] && bdat[bx] > gdat[bx])
				    {   gdat[bx] += 0.5*(rdat[bx] - gdat[bx]);
				        gdat[bx] += 0.5*(bdat[bx] - gdat[bx]);
				    }*/
				    break;
				    
				    case _gbu:
					r = plum + g_l;
				    g = plum + b_l;
				    b = (plum + r_l) * 0.75;
				    ;
				    /*
				    float corr = 1.25;
				    
					r = plum + (corr+1)*g_l - max(0,corr*b_l) - max(0,corr*r_l);
				    g = plum + b_l; // plum + (corr+1)*b_l - corr*r_l;
				    b = plum + r_l; // plum + (corr+1)*r_l - corr*g_l;				    
				    */
				    if (r < 0) r = 0; if (g < 0) g = 0; if (b < 0) b = 0;
				    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
					rdat[bx] = r;
				    gdat[bx] = g;
				    bdat[bx] = b;
				    break;
				    
				    case _gub:
				    
				    if (b_l < g_l)
				        r_l = 0.5 * r_l + 0.5 * b_l;
				    
				    if (b_l < 0) g_l -= b_l;
				    if (g_l > 0) 
				    {   g_l *= 2.5;
				    }
				    
					r = plum + g_l;
				    g = plum + r_l;
				    b = plum + b_l;				    
				    
				    if (r < 0) r = 0; if (g < 0) g = 0; if (b < 0) b = 0;
				    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
				    
					rdat[bx] = r;
				    gdat[bx] = g;
				    bdat[bx] = b;
				    
				    break;
				    
				    case _mon:
				    rdat[bx] = r;
				    gdat[bx] = r;
				    bdat[bx] = r;
				    
				    break;
				    
				    case _raw:
				    rdat[bx] = jdata[ly+x*3  ];
				    gdat[bx] = jdata[ly+x*3+1];
				    bdat[bx] = jdata[ly+x*3+2];
				    
				    break;
				    
				    default:
				    r = plum + r_l;
				    g = plum + g_l;
				    b = plum + b_l;				    
				    
				    if (r < 0) r = 0; if (g < 0) g = 0; if (b < 0) b = 0;
				    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
					rdat[bx] = r;
				    gdat[bx] = g;
				    bdat[bx] = b;
				}
				
				rtot += rdat[bx];
				gtot += gdat[bx];
				btot += bdat[bx];
				
				if (rdat[bx] > rmax) rmax = rdat[bx];
				if (gdat[bx] > gmax) gmax = gdat[bx];
				if (bdat[bx] > bmax) bmax = bdat[bx];
				
				if (rdat[bx] < rmin) rmin = rdat[bx];
				if (gdat[bx] < gmin) gmin = gdat[bx];
				if (bdat[bx] < bmin) bmin = bdat[bx];
			}
		}
		
		rtot -= wid*hei*rmin;
		gtot -= wid*hei*gmin;
		btot -= wid*hei*bmin;
		
		printf("RGB totals: %i %i %i\n", rtot, gtot, btot);
		
		
		float rgam, ggam, bgam, rmul, gmul, bmul;
		rgam = ggam = bgam = 1;
		int totavg = (rtot + gtot + btot) /3;
		
		// rmax -= rmin;
		
		if (!totavg) totavg++;
		
		if (!rmax) rmax++;
		if (!gmax) gmax++;
		if (!bmax) bmax++;
		
		rmul = (rmax>rmin) ? (255.0 / (rmax-rmin)) : 1;
		gmul = (gmax>gmin) ? (255.0 / (gmax-gmin)) : 1;
		bmul = (bmax>bmin) ? (255.0 / (bmax-bmin)) : 1;
		
		
		rgam = pow((float)rtot / totavg, 0.53);
		ggam = pow((float)gtot / totavg, 0.53);
		bgam = pow((float)btot / totavg, 0.53);
		
		if (0)
		{	rgam /= 0.67;
			ggam /= 0.67;
			bgam /= 0.67;
		}
		
		printf("rgam ggam bgam: %f %f %f\n", rgam, ggam, bgam);
		/* if (rotcol)
		{   rgam /= 1.13;
		    ggam /= 0.71;
		    bgam /= 0.67;
		}
		else
		{   bgam /= 1.13;
		    rgam /= 0.71;
		    ggam /= 0.67;
		} */
		
		printf("rgam ggam bgam: %f %f %f\n", rgam, ggam, bgam);
		
		float desat = 0;
		
		if (hei < 800)
        {
		    for (unsigned int y=0; y<hei; y++)
		    {	int ly = y * perline;
			    int by = y * wid;
			    for (unsigned int x=0; x<wid; x++)
			    {	int lx = x * 3;
				    int bx = by+x;
				    
				    // rdat[bx] -= rmin;
				    
				    rdat[bx] = 246*pow(.00391 * rmul * (rdat[bx]-rmin), rgam);
				    gdat[bx] = 246*pow(.00391 * gmul * (gdat[bx]-gmin), ggam);
				    bdat[bx] = 246*pow(.00391 * bmul * (bdat[bx]-bmin), bgam);
			    }
		    }
		}

		
	}
	else
	{	printf("NoIR-less merge not supported!\n");
		return 2;
	}
	
	
	
	
	
	int maxby = wid*(hei-2);
	int reading;
	
	if (in_therm && 1)
	{	unsigned char rtmp[16*16*8*8], gtmp[16*16*8*8], btmp[16*16*8*8];
		unsigned int wtmp = _THERM_W, htmp = _THERM_H;
		
		if (thermdat)
		{	int tempmin = 0, tempmax = 0, frist = 1;
		
		    int i;
    
            for (i=0; i<768; i++) thmult[i] = 1;
	        
            FILE* pf = fopen("/home/pi/thmult.dat", "r");
            if (pf)
            {   char buffer[1024];
                for (i=0; i<768; i++)
                {   fgets(buffer, 1024, pf);
                    thmult[i] = atof(buffer);
                    if ( !thmult[i] ) thmult[i] = 1;
                }
                fclose(pf);
            }
	
		
#ifdef _MLX90640
			for (int y=0; y<_THERM_H; y++)
			{	int ly = y * _THERM_W;
				for (int x=0; x<_THERM_W; x++)
				{	int lx = ly+x;
					
					int reading = thermdat[lx]; // & 0xffff;
					reading *= thmult[lx];
					/*if (reading & 0x8000) reading -= 0x10000;
					reading = 0.4*reading + 160;*/

#else
			for (int y=8; y<128; y+=16)
			{	int ly = ((y-8) >> 4) * wtmp;
				for (int x=8; x<128; x+=16)
				{	int lx = ly+(x>>4);
				
					reading = thermdat[lx] & 0xfff;
					if (reading & 0x800) reading -= 0x1000;
#endif
					if (frist || (reading < tempmin)) tempmin = reading;
					if (frist || (reading > tempmax)) tempmax = reading;
					frist = 0;
				}
			}
		
#ifdef _MLX90640
			int hei2 = hei/2;
			int wid2 = wid/2;

			for (int y=0; y<_THERM_H; y++)
			{	int ly = y * wtmp;
				
				// printf("Processing line %i offset %i...\n", y, by);
				
				for (int x=0; x<_THERM_W; x++)
				{	int bx = therm_off_x * wid + therm_sz_x * wid * x;
					int by = ((int)(therm_off_y * hei) + (int)(therm_sz_y * hei * y));
				
					int by1 = hei2 + (by-hei2) * cos(therm_rot_rad) + (bx-wid2) * sin(therm_rot_rad);
					int bx1 = wid2 + (bx-wid2) * cos(therm_rot_rad) - (by-hei2) * sin(therm_rot_rad);
					
					by = by1;
					bx = bx1;
					
					
					int tsi = x+_THERM_W*y;
					int bmi = bx + wid*by;
					thermspot[tsi].x = bx;
					thermspot[tsi].y = by;
					thermspot[tsi].r = rdat[bmi];
					thermspot[tsi].g = gdat[bmi];
					thermspot[tsi].b = bdat[bmi];
					
				
					int lx = ly+x;
					reading = thermdat[lx]; 
					reading *= thmult[lx];
					char *rgb;
								   
					// therm_mode == _THM_FIRE
					switch (therm_mode)
					{	case _THM_FIRE:
						rgb = fire_grad(reading, tempmin, tempmax);
						break;
						
						case _THM_FEVR:
						rgb = rgb_from_temp_fever(reading);
						break;
						
						case _THM_BLEU:
						case _THM_TIV:
						rgb = bleu_grad(reading, tempmin, tempmax);
						break;
						
						case _THM_HUE:
						default:
						rgb = rgb_from_temp(reading);
						break;
					}
					
					thermr[tsi] = rgb[0];
					thermg[tsi] = rgb[1];
					thermb[tsi] = rgb[2];
#else
			for (int y=8; y<128; y+=16)
			{	int ly = ((y-8) >> 4) * wtmp;
				int by = ((int)(therm_off_y * hei) + (int)(therm_sz_y * hei * (y/16)));
				
				// printf("Processing line %i offset %i...\n", y, by);
				
				for (int x=8; x<128; x+=16)
				{	int bx = therm_off_x * wid + therm_sz_x * wid * (x/16);
					int lx = ly+(x>>4);
					char *rgb;
					
					// therm_mode == _THM_FIRE
					switch (therm_mode)
					{	case _THM_FIRE:
						rgb = fire_grad(reading, tempmin, tempmax);
						break;
						
						case _THM_FEVR:
						rgb = rgb_from_temp_fever(reading);
						break;
						
						case _THM_BLEU:
						case _THM_TIV:
						rgb = bleu_grad(reading, tempmin, tempmax);
						break;
						
						case _THM_HUE:
						default:
						rgb = rgb_from_temp(reading);
						break;
					}
					
					int tsi = ((x-8)>>4)+_THERM_W*((y-8)>>4);
					int bmi = bx + wid*by;
					thermspot[tsi].x = bx;
					thermspot[tsi].y = by;
					thermspot[tsi].r = rdat[bmi];
					thermspot[tsi].g = gdat[bmi];
					thermspot[tsi].b = bdat[bmi];
					
					thermr[tsi] = rgb[0];
					thermg[tsi] = rgb[1];
					thermb[tsi] = rgb[2];
					
#endif
					
#ifdef _MLX90640
					int reading = thermdat[lx] & 0xffff;
					if (reading & 0x8000) reading -= 0x10000;
					reading = 0.4*reading + 160;
					reading *= thmult[lx];
#else
					int reading = thermdat[lx] & 0xfff;
					if (reading & 0x800) reading -= 0x1000;
#endif
					
					// printf("Processing pixel %i bx %i...\n", x, bx);
					
					
					if (!thermfill)
					{
						for (int sqy = by - sq_hsz; sqy < by + sq_hsz; sqy++)
						{	if (sqy < 0 || sqy >= maxby) continue;
							int by2 = sqy * wid;
							for (int sqx = bx - sq_hsz; sqx < bx + sq_hsz; sqx++)
							{	if (sqx < 0 || sqx >= wid) continue;
								int bx2 = by2+sqx;
								
								rdat[bx2] = rgb[0];
								gdat[bx2] = rgb[1];
								bdat[bx2] = rgb[2];
							}
						}
					
					}
					else
					{		;
					}					
					
				}
			}
			
			_exit_thermxy:
			;
			
			
			if (thermfill)
			{	for (int sqy = thermspot[0].y - hei*0.5*therm_sz_y; sqy < thermspot[767-80].y + hei*0.5*therm_sz_y; sqy++)
				{	if (sqy < 0 || sqy >= maxby) continue;
					int by2 = sqy * wid;
					for (int sqx = thermspot[0].x - wid*0.5*therm_sz_x; sqx < thermspot[767-1].x + wid*0.5*therm_sz_x; sqx++)
					{	if (sqx < 0 || sqx >= wid) continue;
						int bx2 = by2+sqx;
						
						pixel_5d p5;
						p5.x = sqx;
						p5.y = sqy;
						p5.r = rdat[bx2];
						p5.g = gdat[bx2];
						p5.b = bdat[bx2];
						
						
#if 0
                        int tsi = nearest_thermspot_5d(p5);
                        
                        if (therm_mode == _THM_TIV)
						{   bdat[bx2] = 0.5*bdat[bx2] + 0.5*gdat[bx2];
						    gdat[bx2] = rdat[bx2];
						    rdat[bx2] = thermr[tsi];
						}
						else
						{   float mxrgb = rdat[bx2];
						    if (gdat[bx2]> mxrgb) mxrgb = gdat[bx2];
						    if (bdat[bx2]> mxrgb) mxrgb = bdat[bx2];

						    mxrgb += _thermlite;
						    mxrgb /= (261 + _thermlite);	
						    if (mxrgb > 1) mxrgb = 1;
						    
						    rdat[bx2] = mxrgb * thermr[tsi];
						    gdat[bx2] = mxrgb * thermg[tsi];
						    bdat[bx2] = mxrgb * thermb[tsi];
						}
                        
#else
						
						wghidx w = blur_thermspot(p5);
						
						float wr=0, wg=0, wb=0;
						
						if (therm_mode == _THM_TIV)
						{   for (int i=0; i<4; i++)
						    {	wr += w.wt[i] * (0.4*thermr[w.idx[i]] + 0.6*thermg[w.idx[i]]);
						    }
						    
						    if (wr > 255) wr = 255;
						    
						    bdat[bx2] = 0.5*bdat[bx2] + 0.5*gdat[bx2];
						    gdat[bx2] = rdat[bx2];
						    rdat[bx2] = wr;
						}
						else
						{   for (int i=0; i<4; i++)
						    {	wr += w.wt[i] * thermr[w.idx[i]];
							    wg += w.wt[i] * thermg[w.idx[i]];
							    wb += w.wt[i] * thermb[w.idx[i]];
						    }
						    
						    float mxrgb;
						    /*
						    mxrgb = rdat[bx2];
						    if (gdat[bx2]> mxrgb) mxrgb = gdat[bx2];
						    if (bdat[bx2]> mxrgb) mxrgb = bdat[bx2];
						    */
						    
						    mxrgb = 0.33*rdat[bx2]
						          + 0.34*gdat[bx2]
						          + 0.33*bdat[bx2]
						          ;
						    
						    mxrgb += _thermlite;
						    mxrgb /= (261 + _thermlite);	
						    if (mxrgb > 1) mxrgb = 1;
						    
						    rdat[bx2] = mxrgb * wr;
						    gdat[bx2] = mxrgb * wg;
						    bdat[bx2] = mxrgb * wb;
						}
						
#endif
						
					}
				}
				
			}
			
			
			
			
		}
	}
	
	
	int status;
	status = 0;
	
	// Crop thermal image
	if (in_therm && thermdat)
	{
		unsigned char *rdat1, *gdat1, *bdat1;
		
		int cropx = therm_off_x * wid;
		int cropy = therm_off_y * hei;
		int cropw = (therm_sz_x)*(_THERM_W-2)*wid;
		int croph = (therm_sz_y)*(_THERM_H-3)*hei;
		
		int pixels = cropw * croph;
		rdat1 = (unsigned char *)malloc(pixels);
		gdat1 = (unsigned char *)malloc(pixels);
		bdat1 = (unsigned char *)malloc(pixels);
		
		for (int y=0; y<croph; y++)
		{	int y1 = y*cropw;
			int y2 = (y+cropy)*wid;
			for (int x=0; x<cropw; x++)
			{	int x1 = y1+x;
				int x2 = y2+x+cropx;
				rdat1[x1] = rdat[x2];
				gdat1[x1] = gdat[x2];
				bdat1[x1] = bdat[x2];
			}
		}
		
		rdat = rdat1; gdat = gdat1; bdat = bdat1;
		wid=cropw; hei=croph;
	}
	
	
	if (!strcmp(out_comp+strlen(out_comp)-4, ".bmp"))
		mk_bmp(wid, hei, 1, rdat, gdat, bdat, out_comp);
	else
	{
		// make a PNG output
		bitmap_t outimg;
		int x;
		int y;

		/* Create an image. */

		outimg.width = wid;
		outimg.height = hei;

		outimg.pixels = calloc (outimg.width * outimg.height, sizeof (pixel_t));

		if (! outimg.pixels) 
		{   printf("Failed to allocate output pixels.\n");
			return -1;
		}

	
		for (y = 0; y < outimg.height; y++) 
		{   for (x = 0; x < outimg.width; x++) 
		    {   pixel_t * pixel = pixel_at (& outimg, x, y);
		        pixel->red   = rdat[x+wid*y];
		        pixel->green = gdat[x+wid*y];
		        pixel->blue  = bdat[x+wid*y];
		    }
		}

		/* Write the image to a file 'outimg.png'. */

		if (save_png_to_file (& outimg, out_comp)) 
		{   fprintf (stderr, "Error writing %s.\n", out_comp);
			status = -1;
		}
		
		free(outimg.pixels);
	}
	
	
	if (del_inp_f)
	{   char cmdb[1024];
	    sprintf(cmdb, "sudo rm %s", in_ugb);
	    system(cmdb);
	}
	
	
	// create an output thumbnail
	char* otnfn = thumb_name(out_comp);
	
	printf("Thumbnail: %s\n", otnfn);
	
#define _THUMBWID 180
#define _THUMBHEI 120

	unsigned int rtn[_THUMBWID*_THUMBHEI],
	             gtn[_THUMBWID*_THUMBHEI],
	             btn[_THUMBWID*_THUMBHEI],
	             dvb[_THUMBWID*_THUMBHEI];
	float scx = (float)_THUMBWID/wid;
	float scy = (float)_THUMBHEI/hei;
	
	int twid = _THUMBWID, thei = _THUMBHEI;
	
	for (int i=0; i<_THUMBWID*_THUMBHEI; i++) 
	    dvb[i] = rtn[i] = gtn[i] = btn[i] = 0;
	
	for (int y=0; y<hei; y++)
	{   int y1 = y*wid;
	    int dy = y * scy;
	    int dy1 = dy*twid;
	    
	    for (int x=0; x<wid; x++)
	    {   int x1 = x + y1;
	        int dx = x * scx;
	        int dx1 = dx + dy1;
	        
	        rtn[dx1] += rdat[x1];
	        gtn[dx1] += gdat[x1];
	        btn[dx1] += bdat[x1];
	        dvb[dx1]++;
	    }
    }
    
    unsigned char rtnc[_THUMBWID*_THUMBHEI],
	              gtnc[_THUMBWID*_THUMBHEI],
	              btnc[_THUMBWID*_THUMBHEI];
	              
    for (int i=0; i<_THUMBWID*_THUMBHEI; i++) 
	{   float invdvb = 1.0 / dvb[i];
	    rtnc[i] = rtn[i] * invdvb;
	    gtnc[i] = gtn[i] * invdvb;
	    btnc[i] = btn[i] * invdvb;
	}
	
	mk_bmp(twid, thei, 1, rtnc, gtnc, btnc, otnfn);
	free(otnfn);
	
	return status;
}










