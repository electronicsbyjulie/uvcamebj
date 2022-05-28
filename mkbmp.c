#include "mkbmp.h"

int mk_bmp(const int width, const int height, const int pxsize, const unsigned char* rdat, const unsigned char* gdat, const unsigned char* bdat, const char* fname)
{
	FILE* pf = fopen(fname, "wb");
	if (!pf) return 1;
	
	int bmhsz, dibhsz, datasz, fsz;
	char buffer[1024];
	int temp;
	
	bmhsz = 0x0e;
	dibhsz = 0x28;
	datasz = width * pxsize * height * pxsize * 3;
	fsz = bmhsz + dibhsz + datasz;
	
	// BMP header
	fwrite("BM", 2, 1, pf);						// magic
	fwrite(&fsz, 1, 4, pf);						// file size
	fwrite("\x00\x00\x00\x00", 4, 1, pf);		// app-dependent info
	temp = bmhsz+dibhsz;
	fwrite(&temp, 1, 4, pf);					// data block offset
	
	// DIB header
	fwrite(&dibhsz, 1, 4, pf);					// header size
	temp = width*pxsize;
	fwrite(&temp, 1, 4, pf);					// image width
	temp = height*pxsize;
	fwrite(&temp, 1, 4, pf);					// image height
	fwrite("\x01\x00", 2, 1, pf);				// color planes - always 1
	fwrite("\x18\x00", 2, 1, pf);				// bits per pixel
	fwrite("\x00\x00\x00\x00", 4, 1, pf);		// compression method; 0=none
	fwrite(&datasz, 1, 4, pf);					// bitmap data size
	temp = 10;
	fwrite(&temp, 1, 4, pf);					// horizontal pixels per meter
	fwrite(&temp, 1, 4, pf);					// vertical pixels per meter
	fwrite("\x00\x00\x00\x00", 4, 1, pf);		// colors in palette
	fwrite("\x00\x00\x00\x00", 4, 1, pf);		// important colors; ignored
	
	int x, y, i, j, aidx, rowsz;
	
	for (y=height-1; y>=0; y--)
	{
		for (i=0; i<pxsize; i++)
		{
			rowsz = 0;
			for (x=0; x<width; x++)
			{
				aidx = x+width*y;
				for (j=0; j<pxsize; j++)
				{
					// BGR order
					fwrite(&bdat[aidx], 1, 1, pf);
					fwrite(&gdat[aidx], 1, 1, pf);
					fwrite(&rdat[aidx], 1, 1, pf);
					rowsz += 3;
				}
			}
			
			rowsz &= 0x03;
			if (rowsz)
			{
				rowsz = 4-rowsz;
				fwrite("\x00\x00\x00\x00", rowsz, 1, pf);			// pad each row to multiple of 4 bytes
			}
		}
	}
	
	fclose(pf);
	return 0;
}

int read_bmp(   int* width,
                int* height, 
                unsigned char* rdat, 
                unsigned char* gdat, 
                unsigned char* bdat, 
                const char* fname
            )
{   
	FILE* pf = fopen(fname, "rb");
	if (!pf) return 1;
	
	char buffer[1024];
	
	int wid, hei, offset;
	
	// BMP header
	fread(buffer, 10, 1, pf);
	fread(&offset, 4, 1, pf);
	
	// DIB header
	//fread(buffer, 40, 1, pf);
	fread(buffer, 4, 1, pf);
	fread(&wid, 4, 1, pf);
	fread(&hei, 4, 1, pf);
	fread(buffer, 28, 1, pf);
	
	if (offset > 54) fread(buffer, offset-54, 1, pf);
	
	// printf("%d x %d\n", wid, hei); 

	if (width)  *width  = wid;
	if (height) *height = hei;
	
	int l, ll, rowsize, x, y, perln;
	
	/*
	ll = wid * hei;
	for (l=0; l<ll; l++)
	{   fread(buffer, 1, 3, pf);
	    bdat[l] = buffer[0];
	    gdat[l] = buffer[1];
	    rdat[l] = buffer[2];
	    rowsize += 3;
	    
	    if (rowsize >= wid*3)
	    {   rowsize &= 3;
	        rowsize = 3-rowsize;
	        fread(buffer, 1, rowsize, pf);
	        rowsize = 0;
	    }
	}*/
	
	perln = 3*wid;
	if (perln % 4) perln += (4 - (perln % 4));
	for (y = hei-1; y >= 0; y--)                    // bottom to top
	{   rowsize = 0;
	    l = y * wid;
	    for (x=0; x<wid; x++)
	    {   ll = l + x;
	        fread(buffer, 1, 3, pf);
	        bdat[ll] = buffer[0];
	        gdat[ll] = buffer[1];
	        rdat[ll] = buffer[2];

	        rowsize += 3;
	        
	        /*
	        if (rowsize >= wid*3)
	        {   rowsize &= 3;
	            rowsize = 3-rowsize;
	            fread(buffer, 1, rowsize, pf);
	            rowsize = 0;
	        }*/
	    } 
	      
	    if (rowsize % 4)
	    {
	        rowsize &= 3;
            rowsize = 3-rowsize;
            fread(buffer, 1, rowsize, pf);
        }
        rowsize = 0;
	}
	
	
	fclose(pf);
	return 0;
}







            

