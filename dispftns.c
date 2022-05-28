
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dispftns.h"

char grgb[3];

const unsigned int  _therm_level[_THERMCOLORS] 
	= {	0x000, 0x03c, 0x050, 0x064, 0x07c,
		0x094, 0x0a0, 0x0c8, 0x190, 
		0x800, 0xf24, 0xf9c, 0xfff
	  };
	
const unsigned long _therm_color[_THERMCOLORS] 
	= {	0x0000ff, 0x0080ff, 0x00ffff, 0x00e000, 0xccbb80,
		0xff0080, 0xffc0e0, 0xffff00, 0xff0000, 
		0xffffff, 0x000000, 0x800080, 0x0000ff
	  };
	  
const unsigned int  _therm_level_f[_FEVCOLORS] 
	= {	0x000, 0x08E, 0x092, 0x094, 
		0x0a6, 0x800, 0xf24, 0xfff
	  };
	
const unsigned long _therm_color_f[_FEVCOLORS] 
	= {	0x000000, 0x0000ff, 0x00ffff, 0x00ff00,
		0xff0000, 0x000000, 0x000000, 0x000000
	  };

float degc_from_reading(int reading)
{
    if (reading >= 0x000 && reading <= 0x7ff) return 0.25 * reading;
    if (reading >= 0xf24 && reading <= 0xfff) return -55 + 0.25 * (reading - 0xf24);
}

float degf_from_reading(int reading)
{	return degc_from_reading(reading) * 9/5 + 32;
}

int sgn(const float f)
{   if (f < 0) return -1;
    if (f > 0) return 1;
    return 0;
}

float max(float a, float b)
{	return (a>b) ? a : b;
}

float min(float a, float b)
{	return (a<b) ? a : b;
}

unsigned char* rgb_in_minmax(float reading, float min, float max)
{	unsigned char* retval = malloc(3);

	if (fabs(max-min) < 30)
	{	max = 0.5*max + 0.5*min;
		min = max - 15;
		max += 15;
	}
	
	float per = 3.141592653589793238462 *1.6 / (max-min);
	float rscaled = (reading-min)*per-1;
	
	retval[0] = 127-127*cos(rscaled);
	retval[1] = 127-127*cos(rscaled+2);
	retval[2] = 127-127*cos(rscaled+4);
	
	char* rgbabs = rgb_from_temp(0.5*min + 0.5*max);
	retval[0] = 0.5*retval[0] + 0.5*rgbabs[0];
	retval[1] = 0.5*retval[1] + 0.5*rgbabs[1];
	retval[2] = 0.5*retval[2] + 0.5*rgbabs[2];
	
	return retval;
}

unsigned char* fire_grad(float reading, float mn, float mx)
{	float j, k;
	j = (reading-mn) / (float)(mx-mn);
	k = j * 3.1415926535897932384626;
	
	float r, g, b;
	
	r = min(max(pow(0.5-0.5*cos(k*2+.7),2), max(1.2*pow(0.5-0.5*cos(k*2+.3),1.25), 1.2*max(min((j-0.2)*4, 1), 0))) * pow(j,0.5) * pow(j,0.2), 1);
	g = max(min(1, max(0, 3.5*pow(0.5-0.5*cos(k*.8),.75)-1.5)), pow(j, 20)) * pow(j,2.5);
	b = max(min(1,1.2*pow(0.5+0.5*sin(k*2+0.2), 2)) * pow(j,0.2), min(1,1.05*pow(j, 10))) * pow(j,0.01);
	
	float l;
	if (l = 0.42*r + 0.44*g + 0.14*b)		// normally we would use .29*r + .57*g + .14*b, but then we get weird bands that are too bright
	{	float m = pow(j / l, 0.75);
		r *= m; g *= m; b*= m;
		if (r > 1) r = 1;
		if (g > 1) g = 1;
		if (b > 1) b = 1;
	}
	
	if ((r<=b && g<=b && b < .75) || j < 0.2)
	{   g += 0.8153*(0.78-b);
	    b = 0.78;
	}
	
	// if (!r && !g && !b) b = g = 0.5;    

	// char retval[3];
	grgb[0] = r*255;
	grgb[1] = g*255;
	grgb[2] = b*255;
	
	return grgb;
	
}


unsigned char* bleu_grad(float reading, float mn, float mx)
{	float j;
	j = (reading-mn) / (float)(mx-mn);
	
	float r, g, b;
	
	r = pow(j, 1.85);
	g = pow(j, 1.03)*.81;
	b = pow(j, 0.67)*.78;
	
	grgb[0] = r*255;
	grgb[1] = g*255;
	grgb[2] = b*255;
	
	return grgb;
	
}

unsigned char* rgb_from_temp(int temp)
{
	unsigned char* retval = malloc(3);    
	
	temp &= 0xfff;
	
	retval[0] = retval[1] = retval[2] = 0x80;
	for (int i=1; i<_THERMCOLORS; i++)
	{	if (temp >= _therm_level[i-1] && temp <= _therm_level[i])
		{	unsigned int rgb0 = _therm_color[i-1];
			unsigned int rgb1 = _therm_color[i];
			
			float fac1 = (float)(temp - _therm_level[i-1])/(_therm_level[i] - _therm_level[i-1]);
			float fac0 = 1.0 - fac1;
			
			retval[0] = (int)(fac0 * (rgb0 & 0xff0000) + fac1 * (rgb1 & 0xff0000)) >> 16;
			retval[1] = (int)(fac0 * (rgb0 &   0xff00) + fac1 * (rgb1 &   0xff00)) >>  8;
			retval[2] = (int)(fac0 * (rgb0 &     0xff) + fac1 * (rgb1 &     0xff))      ;
		}
	}
	return retval;
}

unsigned char* rgb_from_temp_fever(int temp)
{
	unsigned char* retval = malloc(3);    
	
	temp &= 0xfff;
	
	retval[0] = retval[1] = retval[2] = 0x80;
	for (int i=1; i<_FEVCOLORS; i++)
	{	if (temp >= _therm_level_f[i-1] && temp <= _therm_level_f[i])
		{	unsigned int rgb0 = _therm_color_f[i-1];
			unsigned int rgb1 = _therm_color_f[i];
			
			float fac1 = (float)(temp - _therm_level_f[i-1])/(_therm_level_f[i] - _therm_level_f[i-1]);
			float fac0 = 1.0 - fac1;
			
			retval[0] = (int)(fac0 * (rgb0 & 0xff0000) + fac1 * (rgb1 & 0xff0000)) >> 16;
			retval[1] = (int)(fac0 * (rgb0 &   0xff00) + fac1 * (rgb1 &   0xff00)) >>  8;
			retval[2] = (int)(fac0 * (rgb0 &     0xff) + fac1 * (rgb1 &     0xff))      ;
		}
	}
	return retval;
}



int rgb256(float r, float g, float b)
{	
	if (fabs(r-g) < _GRAYSATLMT & fabs(r-b) < _GRAYSATLMT & fabs(g-b) < _GRAYSATLMT)
	{	int nl = (r+g+b)/28;
		if (nl >= 24) return 15;
		if (nl > 23) nl = 23; if (nl < 0) nl = 0;
		
		return 232+nl;
	}
	else
	{	int nr = r/42.7, ng = g/42.7, nb = b/42.7;
		if (nr > 5) nr = 5; if (ng > 5) ng = 5; if (nb > 5) nb = 5;
		if (nr < 0) nr = 0; if (ng < 0) ng = 0; if (nb < 0) nb = 0;
	
		return 16 + nb + 6*ng + 36*nr;
	}
}


char* thumb_name(char* inpfn)
{   char* otnfn;
    otnfn = (char*)malloc(256);
	strcpy(otnfn, inpfn);
	
	for (int i=strlen(otnfn)-1; i>0; i--)
	{   if (otnfn[i] == '/')
	    {   char ufpos[256];
	        strcpy(ufpos, &otnfn[i]);
	        strcpy(otnfn, ufpos);
	        break;
	    }
	}
	
	for (int i=strlen(otnfn)-1; i>0; i--)
	{   if (otnfn[i] == '.')
	    {   otnfn[i] = 0;
	        break;
	    }
	}
	
	char ufpos[256];
    strcpy(ufpos, otnfn);
	sprintf(otnfn, "/tmp%s_th.bmp", ufpos);
	
	return otnfn;
}






