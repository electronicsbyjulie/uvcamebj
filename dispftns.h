#ifndef _GRAYSATLMT
#define _GRAYSATLMT 30
#endif

#define _THM_HUE  0
#define _THM_FIRE 1
#define _THM_FEVR 2
#define _THM_BLEU 3
#define _THM_TIV  4

float degc_from_reading(int reading);
float degf_from_reading(int reading);
int sgn(const float f);
unsigned char* rgb_from_temp(int temp);
unsigned char* rgb_from_temp_fever(int temp);
unsigned char* rgb_in_minmax(float reading, float min, float max);
unsigned char* fire_grad(float reading, float mn, float mx);
unsigned char* bleu_grad(float reading, float mn, float mx);
int rgb256(float r, float g, float b);

char* thumb_name(char* inpfn);

#define _THERMCOLORS 13
#define _FEVCOLORS 8
// const unsigned int  _therm_level[_THERMCOLORS];
