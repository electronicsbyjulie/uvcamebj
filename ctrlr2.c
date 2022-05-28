#include <gtk/gtk.h>
#include <gmodule.h>
#include <time.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <sys/types.h>
#include <pthread.h>
#include "dispftns.h"
#include "globcss.h"
#include "mkbmp.h"

// for shared memory
#include <sys/ipc.h> 
#include <sys/shm.h> 

// device pixels
// #define SCR_RES_X 480
// #define SCR_RES_Y 320

// screen pixels
#define SCR_RES_X 720
#define SCR_RES_Y 480

// Do not wait for images to process; use linked list
#define _SIMULSHOT 1

// pthread_t threads[256];
// int thread_is_active[256];

typedef struct fname4widget
{   GtkWidget *widg;
    char fname[256];
    int resize;
    GdkPixbuf** dest_pixbuf;
} fname4widget;


cairo_t *gcr;
cairo_t *prevcr;
cairo_t *prevhcr;
GtkWidget *gwidget;
GtkWidget *gwidgetp;
GtkWidget *gwidgetph;
GtkWidget *iplbl;
GdkPixbuf *gpixp, *gpixph, *gpixsv;
int cam_pid;
int flashir, flashv;
int firlit, fvlit;

int shutter=0, burst=1, bursting=0, camres, expmode;
int flashy;
int cpstage=0, cam_on, vid_on=0;
int flashd = 0;
int tmlaps = 0;
int trupl = 0;
int expcomp = 0;

int devbox = 0;
int allowvid = 0;

int ch_mapping = 1;

char ltfn[256], ltfnh[256];

#define _MAX_NOIRTYPE 5

char* ntdisp[_MAX_NOIRTYPE+1] = {"ugb", "gbu", "mono", "bgu", "raw", "bee"};
// char* ntdisph[_MAX_NOIRTYPE+1] = {"rgih", "cirh", "monh", "vegh", "grih"};
char* xposrmod[11] = {"snow",
                      "nightpreview",
                      "backlight",
                      "spotlight",
                      "sports",
                      "beach",
                      "verylong",
                      "antishake",
                      "fireworks"
                     };
char* xposrmdsh[11] = {"day",
                       "night",
                       "backlt",
                       "spotlt",
                       "sport",
                       "beach",
                       "vlong",
                       "noshake",
                       "firewrx"
                      };

static cairo_surface_t *surface = NULL;
static cairo_surface_t *surfacep = NULL;
static cairo_surface_t *surfaceph = NULL;
static cairo_surface_t *surfacesv = NULL;

GtkApplication *app;

GtkWidget *window;
GtkWidget *frame;
GtkWidget *drawing_area;
GtkWidget* preview_area;
GtkWidget* previewh_area;
GtkWidget *grid;

GtkWidget *slideshow;
GtkWidget *slsh_grid;
GtkWidget *slsh_lbut;
GtkWidget *slsh_rbut;
GtkWidget *slsh_view;
GtkWidget *slsh_xbut;
GtkWidget *slsh_dbut;
GtkWidget *slsh_flbl;

GtkWidget *menu;
GtkWidget *menu_grid;
GtkWidget *menu_btncuv;
GtkWidget *menu_btnugb;
GtkWidget *menu_btnmon;
GtkWidget *menu_btnraw;

int slsh_lsidx;
char slsh_cimg[1024];
char delcut[11];

GtkWidget* reslbl;
GtkWidget* shutlbl;
GtkWidget* explbl;
GtkWidget* chbtn;
GtkWidget* xbtn;
GtkWidget* prevgrid;
GtkWidget* resgrid;
GtkWidget* resmbtn;
GtkWidget* respbtn;
GtkWidget* shexpgrid;
GtkWidget* shutgrid;
GtkWidget* shutmbtn;
GtkWidget* shutpbtn;                      // yep there's a shut up button
GtkWidget* flonbtn;
GtkWidget* recbtn;

GtkWidget* ifbtn;
GtkWidget* vfbtn;

int lxbx, lxby;

int have_ip = 0;
int had_ip = 1;             // assume 1 until you know for sure 0
int nopitft = 0;

float thmult[780];         // plenty of RAM so avoid seg faults
int thsamp;
long double last_cam_init;
int lxoff, pxoff, phxoff, shxoff;           // No not Phoenix off! Although it does get hot here.
int pcntused;

int ctdn2camreinit;
int pwrr;

typedef struct llistelem
{
    struct llistelem *listprev, *listnext;
    char* fname;
    int panelidx;
}
llistelem;

llistelem *listfirst;
unsigned int listlen;

void list_add(char* filename, int pnlidx)
{   // create new llistelem
    llistelem *nle = (llistelem*)malloc(sizeof(llistelem));
    nle->fname = (char*)malloc(strlen(filename)+2);
    strcpy(nle->fname, filename);
    nle->panelidx = pnlidx;
    nle->listnext = 0;
    nle->listprev = 0;
    
    // find last item
    if (!listlen) listfirst = 0;
    if (!listfirst) listfirst = nle;
    else
    {   llistelem *listlast = listfirst;
        while (listlast->listnext) listlast = listlast->listnext;
    
        // link new item and last item together
        nle->listprev = listlast;
        listlast->listnext = nle;
    }
    
    listlen++;
    
    print_list();
}


llistelem* list_remove(llistelem *toberemoved)
{   if (!toberemoved || !listlen) return 0;

    // if !listprev, repoint listfirst
    if (!toberemoved->listprev) listfirst = toberemoved->listnext;
    
    // else repoint listprev
    else toberemoved->listprev->listnext = toberemoved->listnext;
    
    // repoint listnext
    if (toberemoved->listnext) 
        toberemoved->listnext->listprev = toberemoved->listprev;
    
    llistelem* lnxt = toberemoved->listnext;
    
    // deallocate
    if (toberemoved->fname) 	free(toberemoved->fname);
    if (toberemoved)		free(toberemoved);
    
    listlen--;
    if (!listlen) listfirst = 0;
    
    print_list();
    
    return lnxt;
}

void print_list()
{
    if (!listlen) listfirst = 0;
    if (!listfirst) printf("\n\nEmpty list.\n\n");
    else
    {   llistelem *listlast = listfirst;
        while (listlast->listnext) 
        {   printf("List item %s panel %i\n",
                   listlast->fname,
                   listlast->panelidx
                  );
            listlast = listlast->listnext;
        };
    }
}




/******************************************************************************/
/* CONVERSION FUNCTIONS                                                       */
/******************************************************************************/

char** array_from_spaces(const char* input, int max_results)
{   char** result;

    // scan the input array until not space
    int i,j,k,n;
    
    k=0;
    result = (char*)malloc(max_results * sizeof(char*));
    for (i=0; input[i] == 32; i++);
    
    for (; input[i]; i++)
    {
        // if zero, exit
        if (!input[i]) break;
        
        // printf("%d %c\n", i, input[i]);
        
        // scan forward until space or zero
        for (j=i+1; input[j] && input[j] != 32; j++) ;
        
        // copy range
        int l = j - i;
        result[k] = malloc(l+2);
        for (n=0; n<l; n++)
        {   result[k][n] = input[i+n];
        }
        result[k][n] = 0;
        
        printf("%s\n", result[k]);
        
        k++;
    
        // resume at end of range
        if (!input[j] || k >= max_results)
        {   result[k] = malloc(2);
            result[k][0] = 0;
            break;
        }
        
        for (i=j+1; input[i] == 32; i++);
        i--;
    }
    
    return result;
}

static int strsort_cmp(const void* a, const void* b) 
{ 
   return strcmp(*(const char**)a, *(const char**)b); 
} 

void strsort(const char* arr[], int n) 
{  qsort(arr, n, sizeof(const char*), strsort_cmp); 
} 


/******************************************************************************/
/* SYSTEM HOUSEKEEPING FUCTIONS                                               */
/******************************************************************************/

int is_process_running(char* pname)
{   char cmd[1024];
    // sprintf(cmd, "ps -ef | grep %s | grep -v %i", pname, getpid());
    sprintf(cmd, "ps -ef | grep %s | grep -v grep", pname);
    
    FILE* pf = popen(cmd, "r");
    if (pf)
    {   int lines = 0;
        char buffer[1024];
        while (fgets(buffer, 1024, pf))
        {   /*if (!strstr(buffer, "grep "))*/ lines++;
        }
        fclose(pf);
        return lines;
    }
    return -1;
}

int get_process_pid(pname)
{   char cmd[1024];
    sprintf(cmd, "ps -ef | grep %s", pname);
    FILE* pf = popen(cmd, "r");
    if (pf)
    {   int lines = 0;
        char buffer[1024];
        while (fgets(buffer, 1024, pf))
        {   if (!strstr(buffer, "grep "))
            {   buffer[14] = 0;
                return atoi(buffer+9);
            }
        }
        fclose(pf);
        return lines;
    }
    return 0;
}

void log_action(char* logmsg)
{   if (!devbox) return;

    FILE* pf = fopen("/home/pi/act.log", "a");
    if (pf)
    {   char buffer[1024];
    
        FILE* pfd;
        pfd = popen("date", "r");
        if (pfd)
        {   fgets(buffer, 1024, pfd);
            fclose(pfd);
        }   else strcpy(buffer, "date failed");     // story of my life
        
        fprintf(pf, "%s: %s\n", buffer, logmsg);
        fclose(pf);
    }
}

/*
 * Check if a file exist using fopen() function
 * return 1 if the file exist otherwise return 0
 * WARNING: VERY KLUDGY! I'm not crediting the author so they can save face.
 */
int cfileexists(const char * filename)
{   /* try to open file to read */
    FILE *file;
    if (file = fopen(filename, "r"))
    {   fclose(file);
        return 1;
    }
    return 0;
}


void check_disk_usage()
{   FILE* fp;
    char buffer[1024];
        
    if (fp = popen("df | grep /dev/root", "r"))
    {   fgets(buffer, 1024, fp);
        printf("%s", buffer);
        char** arr = array_from_spaces(buffer, 7);
        // arr[4][strlen(arr[4]-1)] = 0;
        pcntused = atoi(arr[4]);
        fclose(fp);
    }
}


void raspicam_cmd_format(char* cmd, int signal, int is_vid)
{   char shut[64], *res;
    int lshut;
    
    lshut = shutter;
    if (signal && lshut > 3000000) lshut = 3000000;
    if (lshut > 0)
    {   sprintf(shut, "--shutter %i", lshut);
    }
    else strcpy(shut, "");
    
    switch (camres)
    {   
        case 480:
        res = "-w 640 -h 480";
        break;
        
        case 768:
        res = "-w 1024 -h 768";
        break;
        
        case 1080:
        res = "-w 1920 -h 1080";
        break;
        
        case 600:
        default:
        res = "-w 800 -h 600";
    }
    
    long double t = time(NULL);
    if ((t - last_cam_init) < 2)
    {   // reinit is fucking up again
        // system("/bin/bash /home/pi/reinit_fuctup.sh");
        // system("sudo reboot");
        
        return;
    }
    
    last_cam_init = t;
    
    char* brsparam = // bursting ? " --burst" : "";
                     (burst > 1) ? " --burst" : "";
    
    char* cmdexc = is_vid 
                 // ? "raspivid -t 999999" 
                 ? "raspivid -t 0 -fps 24 -b 5000000" 
                 : "raspistill"
                 ;
    
    char* outfn = is_vid
                ? "/tmp/output.h264"
                // ? "- | ffmpeg -i - -f alsa -ac 1 -i hw:0,0 -map 0:0 -map 1:0 -vcodec copy -acodec aac -strict -2 /tmp/output.flv"
                : "/tmp/output.jpg"
                ;
    
    raspistill_end_misery(signal 
                        ? (is_vid ? "video" : "reinit" )
                        : "long exposure"
                         );
    int iso = min(800, 800+70*expcomp);
    sprintf(cmd, "%s %s -p '192,120,360,240' -op 255 -sa 40 -co -5 -q 100 -drc high %s %s %s -vf -hf -awb greyworld -ex %s -ev %d -ISO %d --drc high -o %s %s"
                , cmdexc
                , signal?"--signal":""
                , res
                , shut
                , brsparam
                , xposrmod[expmode]
                , expcomp
                , iso
                , outfn
                , (signal|is_vid)?"&":""
           );
    log_action(cmd);
}



void raspistill_init()
{   char cmd[1024];

    if (cpstage || !cam_on || vid_on) return;
    
    raspicam_cmd_format(cmd, 1, 0);
    system(cmd);
    if (shutter > 0) usleep(shutter*6.5);
    else usleep(100000);
    cam_pid = vid_on
                ? get_process_pid("raspivid")
                : get_process_pid("raspistill");
    // printf("Camera PID is %i my PID is %i\n", cam_pid, getpid());
    
    sprintf(cmd, "sudo renice -n 2 %i", cam_pid);
    system(cmd);
    
    if (shutter > 427503)
    {   system("sudo pkill fbcp");
        usleep(200000);
        if (!is_process_running("fbcp")) system("fbcp &");
    }
}

void raspistill_end_misery(char* reason)
{   char buffer[1024];
    sprintf(buffer, "pkill raspistill for reason: %s", reason);
    log_action(buffer);
    system("sudo pkill raspistill");
    
    if (shutter > 427503)
    {   system("sudo pkill fbcp");
        usleep(200000);
        if (!is_process_running("fbcp")) system("fbcp &");
    }
}

// https://stackoverflow.com/questions/6898337/determine-programmatically-if-a-program-is-running
pid_t proc_find(const char* name) 
{
    DIR* dir;
    struct dirent* ent;
    char* endptr;
    char buf[512];

    if (!(dir = opendir("/proc"))) 
    {   log_action("can't open /proc");
        return -1;
    }

    while((ent = readdir(dir)) != NULL) 
    {
        /* if endptr is not a null character, the directory is not
         * entirely numeric, so ignore it */
        long lpid = strtol(ent->d_name, &endptr, 10);
        if (*endptr != '\0') 
        {   continue;
        }

        /* try to open the cmdline file */
        snprintf(buf, sizeof(buf), "/proc/%ld/cmdline", lpid);
        FILE* fp = fopen(buf, "r");

        if (fp) 
        {   if (fgets(buf, sizeof(buf), fp) != NULL) 
            {   /* check the first token in the file, the program name */
                char* first = strtok(buf, " ");
                if (!strcmp(first, name)) 
                {   fclose(fp);
                    closedir(dir);
                    return (pid_t)lpid;
                }
            }
            fclose(fp);
        }

    }

    closedir(dir);
    return 0;
}


int check_processes()
{   char buffer[1024];
    
    if (!is_process_running("fbcp"))
    {   nopitft++;
        if (nopitft > 5)
        {   printf("Starting PiTFT...\n");
            system("fbcp &");
            nopitft = 0;
        }
    }
    else nopitft = 0;
    
    DIR* dir = NULL;
    sprintf(buffer, "/proc/%i", cam_pid);
    if (!cam_pid || !(dir = opendir(buffer))) 
    {
        if (cam_on < 0)
        {   cam_on++;
            return 1;
        }
    
        // int result = is_process_running("raspistill");
        int result = proc_find("raspistill");
        if (!result)
        {   system("ps -ef | grep raspistill >> /home/pi/act.log");
            sprintf(buffer, "Process check %i starting raspistill...\n", result);
            log_action(buffer);
            raspistill_init();
            usleep(200000);
            upd_lbl_shut();
        }
        cam_pid = vid_on
                ? get_process_pid("raspivid")
                : get_process_pid("raspistill");
        // printf("Camera PID is %i my PID is %i\n", cam_pid, getpid());
    }
    if (dir) closedir(dir);
    
    return 1;
}


static gboolean force_gdbkp()
{
    if (!have_ip) return FALSE;
    
    if (!trupl)
    {   // Initiate cloud backup and set flag, then set future checks.
        system("sudo /bin/bash /home/pi/gdbkp.sh &");
        trupl = 1;
        return TRUE;
    }   else
    {   // Once cloud backup finishes, clear flag and end checks.
        if (is_process_running("gdbkp")) return TRUE;
        else
        {   trupl = 0;
            return FALSE;
        }
    }
    
    return FALSE;
}

/******************************************************************************/
/* BASIC GRAPHICS STUFF                                                       */
/******************************************************************************/

static void
clear_surface (void)
{
  cairo_t *cr;

  cr = cairo_create (surface);

  cairo_set_source_rgb (cr, 0, 0, 0);
  // cairo_paint (cr);
  cairo_rectangle(cr, 0, 0, SCR_RES_X / 2, SCR_RES_Y / 2);
  cairo_fill(cr);

  cairo_destroy (cr);
}

static void
clear_surfacep (void)
{
  cairo_t *cr;

  cr = cairo_create (surfacep);

  cairo_set_source_rgb (cr, 0, 0, 0);
  // cairo_paint (cr);
  cairo_rectangle(cr, 0, 0, SCR_RES_X / 4, SCR_RES_Y / 4);
  cairo_fill(cr);

  cairo_destroy (cr);
}

static void
clear_surfaceph (void)
{
  cairo_t *cr;

  cr = cairo_create (surfaceph);

  cairo_set_source_rgb (cr, 0, 0, 0);
  // cairo_paint (cr);
  cairo_rectangle(cr, 0, 0, SCR_RES_X / 4, SCR_RES_Y / 4);
  cairo_fill(cr);

  cairo_destroy (cr);
}

static void
clear_surfacev (void)
{
  cairo_t *cr;

  cr = cairo_create (surfacesv);

  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_paint (cr);

  cairo_destroy (cr);
}

GdkPixbuf * draw_image_widget(GtkWidget *da, char* fname, int resz)
{
    GError *err = NULL;
    GdkPixbuf *pix;
    /* Create pixbuf */
    pix = gdk_pixbuf_new_from_file(fname, &err);
    if(err)
    {
        printf("Error : %s\n", err->message);
        g_error_free(err);
        return FALSE;
    }
    
    if (!pix) printf("ERROR pix is null!");
    
    gint w_i, h_i;
    gdk_pixbuf_get_file_info(fname, &w_i, &h_i);
    
    int w, h, x=0;
    gtk_widget_get_size_request(da, &w, &h);
    
    float inrat, outrat;
    inrat  = (float)w_i / h_i;
    outrat = (float)w   / h  ;
    
    if (outrat > inrat) 
    {   x = w;
        w = h * inrat;
        x -= w;
        x /= 2;
    }
    
    if (resz)
    {
        printf("Scaling to %i, %i px, aspect ratio %f, with x offset %i\n", w, h, inrat, x);

        GdkPixbuf *scaled;
        scaled = gdk_pixbuf_scale_simple(pix,
                                         w,
                                         h,
                                         GDK_INTERP_BILINEAR
                                        );
        if (!scaled) printf("ERROR scaled is null!");
        
        
        GdkWindow *draw = gtk_widget_get_window(da);
        
        cairo_t *cr = gdk_cairo_create (draw);
        gdk_cairo_set_source_pixbuf (cr, scaled, x, 0);

        lxoff = x;
        cairo_paint (cr);
        cairo_destroy (cr);
        
        gtk_widget_queue_draw(da);
        
        return scaled;
    }   else
    {
        GdkWindow *draw = gtk_widget_get_window(da);
        
        cairo_t *cr = gdk_cairo_create (draw);
        gdk_cairo_set_source_pixbuf (cr, pix, x, 0);

        lxoff = x;
        cairo_paint (cr);
        cairo_destroy (cr);
        
        gtk_widget_queue_draw(da);
        
        return pix;
    }
}

void img_widg_thread(fname4widget *fn4w)
{   // pthread_t

    printf("Thread drawing image %s on widget %8x...\n",
            fn4w->fname,
            fn4w->widg
          );
          
    // while (!cfileexists(

    GdkPixbuf *pb =
    draw_image_widget(fn4w->widg,
                      fn4w->fname,
                      fn4w->resize
                     );
    
    if (fn4w->dest_pixbuf) *(fn4w->dest_pixbuf) = pb;
    
    usleep(100000);
    
    if (fn4w) free(fn4w);
    printf("Exiting thread.\n");
    pthread_exit(NULL);
}



/******************************************************************************/
/* UI UPDATERS                                                                */
/******************************************************************************/

void
offer_delete_response (GtkDialog *dialog,
               gint       response_id,
               gpointer   user_data)
{   char** cleanls = {"ls -1 /home/pi/Pictures/",
					  "ls -1 /home/pi/Videos/"
					 };
					 
    if (response_id == -8)          // yes
    {   FILE* fp;
    
    	for (int i=0; i<2; i++)
    	{	fp = popen(cleanls[i],"r");
		    char buffer[256], cmdbuf[256];
		    if (fp)
		    {   while (!feof(fp))
		        {   fgets(buffer, 256, fp);
		            if (buffer[8] == '.')
		            {   buffer[8] = 0;
		                if (strcmp(buffer, delcut) <= 0)
		                {   buffer[8] = '.';
		                    sprintf(cmdbuf, 
		                            "sudo rm /home/pi/Pictures/%s",
		                            buffer
		                           );
		                    system(cmdbuf);
		                }
		                else break;
		            }
		        }
		        fclose(fp);
		    }
        }
    }
    
    cam_on = 1;
    raspistill_init();
}

void offer_delete()
{   char buffer[256], buf1[256], buf2[256];
    
    strcpy(buf1, "00000000");
    strcpy(buf2, "1970 Jan 01");
    FILE* fp = popen("/bin/bash /home/pi/medianpic.sh", "r");
    if (fp)
    {   fgets(buf1, 256, fp);
        fgets(buf2, 256, fp);
        fclose(fp);
    }
    
    for (int i=0; i<256; i++)
    {   if (buf1[i] < 32)
        {   buf1[i] = 0;
            break;
        }
    }
    
    strcpy(delcut, buf1);
    
    if (pcntused < 99)
    {   sprintf(buffer, 
                "Memory is %i%s full. \nDelete photos from\n%sand older?", 
                pcntused, 
                "%%",
                buf2
               );
    }
    else
    {   sprintf(buffer, 
                "MEMORY FULL. \nDelete photos from\n%sand older?", 
                buf2
               );
    }
    
    cam_on = 0;
    raspistill_end_misery("Memory usage message");
    
    GtkWidget * dlg = gtk_message_dialog_new (window,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_YES_NO,
        buffer
        );
        
    g_signal_connect(dlg, "response",
                     G_CALLBACK (offer_delete_response), 
                     NULL
                    );

    gtk_dialog_run (GTK_DIALOG (dlg));
    gtk_widget_destroy (dlg);
}


void upd_btn_pwr()
{	GtkStyleContext *context;
    context = gtk_widget_get_style_context(xbtn);
    if (pwrr)
    {	gtk_style_context_remove_class(context, "pwr");      
    	gtk_style_context_add_class(context, "pwrr");
	}
	else
	{	gtk_style_context_remove_class(context, "pwrr");      
    	gtk_style_context_add_class(context, "pwr");
	}
}

void upd_lbl_shut()
{   char buffer[10];
    sprintf(buffer, "Burst: %d", burst);
    gtk_button_set_label(shutlbl, buffer);
    ctdn2camreinit = 13;
}


void upd_lbl_exp()
{   return;
    char buffer[16];
    sprintf(buffer, "Exp: %s", xposrmdsh[expmode]);
    gtk_button_set_label(explbl, buffer);   
}

void upd_lbl_expcomp()
{   char buffer[16];
    sprintf(buffer, "%d", expcomp);
    gtk_button_set_label(vfbtn, buffer);  
}

void upd_lbl_res()
{
    switch (camres)
    {   
        case 480:
        gtk_label_set_text(reslbl, "640\nx480");
        break;
        
        case 768:
        gtk_label_set_text(reslbl, "1024\nx768");
        break;
        
        case 1080:
        gtk_label_set_text(reslbl, "1080p");
        break;
        
        case 600:
        default:
        gtk_label_set_text(reslbl, "800\nx600");
    }
    
}


int thingdraw()
{   int dx, dy;
	cairo_t *lcr;
	
	if (pwrr) 
	{   pwrr--;
	    if (!pwrr) upd_btn_pwr();
	}
	

    lcr = cairo_create (surface);

    cairo_set_source_rgb(lcr, 0, 0, 0);
    cairo_rectangle(lcr,  0, dy, SCR_RES_X, SCR_RES_Y);
    cairo_fill (lcr);
    
    cairo_rectangle(lcr, dx,  0, SCR_RES_X, SCR_RES_Y);
    cairo_fill (lcr);
    
    cairo_destroy (lcr);

    /* Now invalidate the affected region of the drawing area. */
    gtk_widget_queue_draw(gwidget);
    
    
    if (ctdn2camreinit)
    {   ctdn2camreinit--;
        if (!ctdn2camreinit)
        {   raspistill_init();
        }
    }
    
    
#if _SIMULSHOT

    int counter = listlen;
    if (listfirst)
    {   llistelem *le = listfirst;
        while (le && counter)
        {   if (!is_process_running("imgcomb") && cfileexists(le->fname))
            {   
                // while (is_process_running("imgcomb")) usleep(100000);
                
                printf("Applying image %s with panel id %i\n",
                       le->fname,
                       le->panelidx
                      );
                
                
                
                pthread_t ptt;
                fname4widget *f4w;
                
                f4w = (fname4widget*)malloc(sizeof(fname4widget));
                
                switch (le->panelidx)
                {   case 1:
                    f4w->widg = gwidgetph;
                    strcpy(f4w->fname, le->fname);
                    f4w->resize = 0;
                    f4w->dest_pixbuf = &gpixph;
                    
                    if ( pthread_create(&ptt, 
                                         NULL, 
                                         img_widg_thread, 
                                         f4w
                                        )
                       )
                    {   printf("FAILED TO CREATE THREAD\n");
                        gpixph = draw_image_widget(gwidgetph, 
                                                  le->fname, 
                                                  0
                                                 );
                    
                    }
                    
                    // phxoff = lxoff;
                    break;
                    
                    case 0:
                    // gpixp  = draw_image_widget(gwidgetp, le->fname, 0);
                    f4w->widg = gwidgetp;
                    strcpy(f4w->fname, le->fname);
                    f4w->resize = 0;
                    f4w->dest_pixbuf = &gpixp;
                    
                    if ( pthread_create(&ptt, 
                                         NULL, 
                                         img_widg_thread, 
                                         f4w
                                        )
                       )
                    {   printf("FAILED TO CREATE THREAD\n");
                        gpixp = draw_image_widget(gwidgetp, 
                                                  le->fname, 
                                                  0
                                                 );
                    
                    }
                    // pxoff = lxoff;
                    break;
                    
                    default:
                    ;
                }

		// if (f4w) free(f4w);
                
                le = list_remove(le);
                counter--;
            }   
            else 
            {   /* printf("Image not ready %s for panel id %i\n",
                       le->fname,
                       le->panelidx
                      );
                */
                
                le = le->listnext;
            }
            
            counter--;
        }
    }
    
    
#endif

    

    return 1;
}


void
delete_old_response (GtkDialog *dialog,
               gint       response_id,
               gpointer   user_data)
{   // printf("Response: %i\n", response_id);
    // system("sudo pkill raspistill");
    // kill(getpid(),SIGINT);
    
    gtk_window_iconify(dialog);
    
    if (response_id == -8)          // yes
    {   char cmdbuf[1024], buffer[1024], filedate[1024];
        
        FILE *pf, *pf1;
        
        pf1 = popen("date -d \"yesterday 00:00\" +\x25Y\x25m\x25\x64", "r");
        if (pf1)
        {   while (fgets(buffer, 1024, pf1))
            {   if (buffer[0] == '2')           // NOT Y3K COMPLIANT
                {   strcpy(filedate, buffer);
                }
            }
            fclose(pf1);
        }
        
        pf = popen("ls /home/pi/Pictures/*.png | sort", "r");
        if (pf)
        {   while (fgets(buffer, 1024, pf))
            {   if (strcmp(&buffer[18], filedate) < 0)
                {
                    sprintf(cmdbuf, "sudo rm %s", buffer);
                    // sprintf(cmdbuf, "mv %s /home/pi/Pictures/test/", buffer);
                    system(cmdbuf);
                    // printf("%s\n", cmdbuf);
                }
            }
            
            fclose(pf);
        }
    }
    
    cam_on = 1;
}


int redraw_btns()
{
    /*if (flashd) 
    {	if (ltfn[0]) gtk_widget_queue_draw(preview_area);
    	flashd--;
	}*/
	
	
    gtk_widget_queue_draw(chbtn);
    gtk_widget_queue_draw(xbtn);
    
    
    FILE *pf;
    
    
    pf = popen("ls /media/pi/", "r");
    if (pf)
    {
        char buffer[1000];
        buffer[0] = 0;
        fgets(buffer, 1000, pf);
        fclose(pf);
    
        if (strlen(buffer))
        {   cpstage++;
            
            switch(cpstage)
            {   case 1:
                gtk_label_set_text(iplbl, "Copying files...");
                return 1;
                
                case 2:
                cam_on = 0;
                printf("Copying files to %s...\n", buffer);
                // system("sudo pkill raspistill");
                raspistill_end_misery("copying to USB device");
                system("/bin/bash /home/pi/usbcp.sh");
                
                while (is_process_running("usbcp.sh"))
                {   usleep(100000);
                }
                
                GtkWidget * dlg = gtk_message_dialog_new (slideshow,
                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                    GTK_MESSAGE_WARNING,
                    GTK_BUTTONS_OK,
                    "Please remove the USB device."
                    );
                
                
                gtk_dialog_run (GTK_DIALOG (dlg));
                gtk_widget_destroy (dlg);
                
                
                dlg = gtk_message_dialog_new (slideshow,
                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                    GTK_MESSAGE_WARNING,
                    GTK_BUTTONS_YES_NO,
                    "Delete old photos\n(> 1 day)?"
                    );
                
                g_signal_connect(dlg, "response",
                                  G_CALLBACK (delete_old_response), NULL);

                gtk_dialog_run (GTK_DIALOG (dlg));
                gtk_widget_destroy (dlg);
                
                
                return 1;
                
                case 3:
                gtk_label_set_text(iplbl, "Safe to rmv USB.");
                cpstage=0;
                break;
                
                default:
                ;
            }
        }
        else cpstage=0;
    }
    
    
    
    
    // IP address
    pf = popen("ifconfig | grep \"inet \" | grep \"cast \"", "r");
    if (pf)
    {
        char buffer[1000];
        int i, j;
        // for (i=0; i<256; i++) buffer[i] = 0;
        
        fgets(buffer, 1000, pf);
        fclose(pf);
        
        int found=0;
        if (strlen(buffer)>6) // && strchr(buffer, '.'))
        {   for (i=1; i<256; i++)
            {   
                if (buffer[i] >= '0' && buffer[i] <= '9'
                   ) 
                {   found++;
                    break;
                }
            }
            
            
            
            if (found)
            {
                for (j=i; j<i+16; j++)
                {   if (buffer[j] != '.'
                        && (buffer[j] < '0' || buffer[j] > '9')
                       )
                    {   buffer[j] = 0;
                        break;
                    }
                }
            }
            
            buffer[i+16] = 0;
        }
                
        time_t rt;
        struct tm *lt;
        time(&rt);
        lt = localtime(&rt);
        
        if (found)
        {   if (!strchr(&buffer[i], '.')) found = 0;
            if (strlen(&buffer[i]) < 7) found = 0;
        }
        
        char buffer2[1024];
        
        if (found)
        {   had_ip = have_ip;
            have_ip = 1;
            
            // if (!had_ip && have_ip && !is_process_running("gdbkp")) system("/bin/bash /home/pi/gdbkp.sh &");
            
        	if (is_process_running("gdbkp"))
            {   /*int j = strlen();
                strcpy(&buffer[i+j], " ^");*/
                sprintf(buffer2, 
                        "%s%02i:%02i:%02i %s %i%%^", 
                        devbox ? "$" : "",
                        lt->tm_hour,
                        lt->tm_min,
                        lt->tm_sec,
                        &buffer[i],
                        pcntused
                       );
            }   else
            {   sprintf(buffer2, 
                        "%s%02i:%02i:%02i %s %i%%", 
                        devbox ? "$" : "",
                        lt->tm_hour,
                        lt->tm_min,
                        lt->tm_sec,
                        &buffer[i],
                        pcntused
                       );
            }
            // printf("%s\n", buffer2);
            gtk_label_set_text(iplbl, buffer2);
        }
        else
        {   
            sprintf(buffer2, 
                    "%s%02i:%02i:%02i %s %i%%", 
                    devbox ? "$" : "",
                    lt->tm_hour,
                    lt->tm_min,
                    lt->tm_sec,
                    "No Internet",
                    pcntused
                   );
            gtk_label_set_text(iplbl, buffer2);
        }
    }
    
    return 1;
}


void slsh_view_oldest()
{   FILE* pf = popen("ls /home/pi/Pictures/*.png | sort -r", "r");
    if (pf)
    {   char buffer[512];
        int i=0;
        
        while (fgets(buffer, 512, pf))
        {   i++;
        }
        
        slsh_lsidx = i-1;
        slsh_view_file();
        
        fclose(pf);
    }
}


void slsh_view_file()
{   printf("Opening ls for images.\n");

	FILE* pf;
	char buffer[512];
	int dirlen;
    
	pf = popen("ls /home/pi/Pictures | wc -l | sed 's/[^0-9]*//g'", "r");
	if (pf)
	{	fgets(buffer, 512, pf);
		fclose(pf);
		dirlen = atoi(buffer);
		if (slsh_lsidx >= dirlen) slsh_lsidx = 0;
	}

    pf = popen("ls /home/pi/Pictures/*.png | sort -r", "r");
    if (pf)
    {   int i;
        
        strcpy(buffer, "");
                
        printf("Checking file existence 1/2.\n");
        if (ltfn[0] > 32)
        {   if (!cfileexists(ltfn)) 
            {   strcpy(ltfn, "");
            }
        }
                
        printf("Checking file existence 2/2.\n");
        if (ltfnh[0] > 32)
        {   if (!cfileexists(ltfnh)) 
            {   strcpy(ltfnh, "");
            }
        }
        
        printf("Checking file index.\n");
        if (slsh_lsidx < 0 && !ltfn[0]) slsh_lsidx = -1-slsh_lsidx;

        
        /*
        printf("ltfn = %s\n", ltfn);
        if (slsh_lsidx == -1)
        {   strcpy(slsh_cimg, ltfn);
            goto _have_fn_already;
        }
        if (slsh_lsidx == -2)
        {   strcpy(slsh_cimg, ltfnh);
            goto _have_fn_already;
        }
        */
        
        printf("slsh_lsidx = %i\n", slsh_lsidx);
        
        char very1st[1024];
        printf("Iterating results.\n");
        for (i=0; slsh_lsidx<0 || i<=slsh_lsidx; i++)
        {   fgets(buffer, 512, pf);
            buffer[strlen(buffer)-1] = 0;
            
            if (!i) strcpy(very1st, buffer);
            printf("lsidx %i is %s\n", i, buffer);
            
            if (slsh_lsidx == -1 && !strcmp(ltfn, buffer)) break;
            if (slsh_lsidx == -2 && !strcmp(ltfnh, buffer)) break;
            
            if (feof(pf))
            {   strcpy(buffer, very1st);
                slsh_lsidx = 0;
                break;
            }
        }
        
        if (slsh_lsidx >= 0)
        {   // fgets(buffer, 512, pf);
        }
        else slsh_lsidx = i;
        
        printf("Nulling empty buffer.\n");
        if (strlen(buffer) && buffer[strlen(buffer)-1] < 32) buffer[strlen(buffer)-1] = 0;
        
        printf("Storing image filename.\n");
        strcpy(slsh_cimg, buffer);
        
_have_fn_already:
        printf("Displaying image %s\n", slsh_cimg);
        if(!slsh_view) printf("ERROR slsh_view is null\n");
        printf("Drawing image widget %s.\n", slsh_cimg);
        gpixsv = draw_image_widget(slsh_view, slsh_cimg, 1);
        gtk_label_set_text(slsh_flbl, slsh_cimg+18);
        shxoff = lxoff;
        if (!gpixsv) printf("ERROR gpixsv is null\n");
        printf("Queueing image widget draw %s.\n", slsh_cimg);
        usleep(42778);
        gtk_widget_queue_draw(slsh_view);
        printf("Queued image widget draw %s.\n", slsh_cimg);
    }
}




/******************************************************************************/
/* CAMERA FUCTIONALITY                                                        */
/******************************************************************************/

static gboolean
flash(void)
{
  cairo_t *cr;
  char filename[1024];
  char buffer[1024];
  char tmpfn[256];
  
  
  if (flashv ) { system("gpio write 4 0"); firlit = 1; }
  if (flashir) { system("gpio write 0 0"); fvlit  = 1; }
    
  printf("Doing flash..\n");
  
  system("sudo rm /tmp/output.jpg");
  
  
      
      
      FILE* pf = popen("date +\x25N", "r");
        if (pf)
        {   while (fgets(buffer, 1024, pf))
            {   if (buffer[0] >= '0' && buffer[0] <= '9')     
                {
                    // sprintf(tmpfn, "tmp%i.%i.jpg", atoi(buffer), bi);
                    break;
                }
            }
            fclose(pf);
        }
        
  /*if (burst > 1)
  {     raspistill_end_misery("burst mode");
        bursting=1;
        raspistill_init();
        usleep(250000); 
  }*/
  for (int bi=1; bi<=burst; bi++)
  {
        // sprintf(buffer, "ps -ef | grep %i >> /home/pi/act.log", cam_pid);
        // system(buffer);
        // sprintf(buffer, "Killing SIGUSR1 to process %i", cam_pid);
        // log_action(buffer);
        kill(cam_pid, SIGUSR1);
        // log_action("Copying shared memory");
        
        
        if (shutter < 13881503 || vid_on)
        {
        system("/home/pi/readshm 70 2048 int cp 73 768");
        } else
        { raspicam_cmd_format(buffer, 0, 0);
        raspistill_end_misery("taking long exposure");
        system(buffer);
        raspistill_init();
        }
        
        cr = cairo_create (surface);
        
        while (!cfileexists("/tmp/output.jpg"))
        {  usleep(50000); 
        }
        
        sprintf(tmpfn, "tmp%i.%i.jpg", atoi(buffer), bi);
        sprintf(buffer, "sudo mv /tmp/output.jpg /tmp/%s", tmpfn);
        system(buffer);
    }
    bursting=0;
    if (burst > 1) 
    {   raspistill_end_misery("return to preview");
        raspistill_init();
    }

    /* if (pcntused >= 80) */ check_disk_usage();
    if (pcntused >= 98)
    {   offer_delete();
        return;
    }
    
    system("gpio write 4 1");
    system("gpio write 0 1");
    firlit = fvlit = 0;
  
    pf = popen("date +\x25Y\x25m\x25\x64.\x25H\x25M\x25S.\x25N", "r");
    if (pf)
    {   int lines = 0;
        while (fgets(buffer, 1024, pf))
        {   if (buffer[0] == '2')           // NOT Y3K COMPLIANT
            {   strcpy(filename, buffer);
                int i;
                for (i=0; i<1023; i++)
                {   if (filename[i] <= ' ') 
                    {   filename[i] = 0;
                        i += 1024;
                    }
                }
                sprintf(buffer, "Timestamp: %s\n", filename);
                // log_action(buffer);
                goto _thing;
                break;
            }
        }
        _thing:
        ;
        fclose(pf);
    }
  
  /*
  cairo_set_source_rgb (cr, 1, 1, 0);
  cairo_paint (cr);
  gtk_widget_queue_draw_area (gwidget, 0, 0, SCR_RES_X, SCR_RES_Y);
  
  usleep(200000);
  
  cairo_set_source_rgb (cr, 1, 0, 0);
  cairo_paint (cr);
  gtk_widget_queue_draw_area (gwidget, 0, 0, SCR_RES_X, SCR_RES_Y);
  
  usleep(200000);

  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_paint (cr);
  gtk_widget_queue_draw_area (gwidget, 0, 0, SCR_RES_X, SCR_RES_Y);
  
  */
  // usleep(100000);
  
  if (filename)
  { char cmdbuf[1024], *ampersand;
    char fn[256], fnh[256];
    
    sprintf(fn, 
            "/home/pi/Pictures/%s.%s.png",
            filename,
            ntdisp[ch_mapping] // ? "cir" : "rgi"
           );
           
    char* fnth = thumb_name(fn);
    
    strcpy(ltfn, fn);
    
#if _SIMULSHOT
    list_add(fnth, 0);
#else        
    // strcpy(ltfn, fn);
#endif
   
#if _SIMULSHOT
    ampersand = "&";
#else
    // If taking 1080p, wait for processing before allow more pictures
    ampersand = "";
#endif
       
    
    char* noirarg;
    switch (ch_mapping)
    {   case 0:
        noirarg = "-ugb";
        break;
        
        case 2:
        noirarg = "-mono";
        break;
        
        case 3:
        noirarg = "-bgu";
        break;
        
        case 4:
        noirarg = "-raw";
        break;
        
        case 5:
        noirarg = "-bee";
        break;
        
        case 1:
        default:
        noirarg = "-gbu";     // uvcamebj
    }
    
    char brst[10];
    if (burst <= 1) strcpy(brst, "");
    else sprintf(brst, " -b %d", burst);
    
    sprintf(buffer, "Output filename: %s\n", fn);
    log_action(buffer);
    sprintf(cmdbuf, 
        "/home/pi/imgcomb -iugb /tmp/%s %s %s -o %s %s",
        tmpfn,
        noirarg,
        brst,
        fn, 
        ampersand
        );    
    log_action(cmdbuf);
    // printf("%s\n", cmdbuf);
    system(cmdbuf);
    
    
    
    // if (ch_mapping == 2) usleep(300000);
    
#if _SIMULSHOT
    ;
#else
    while (is_process_running("imgcomb")) usleep(100000);
    
    gpixp  = draw_image_widget(gwidgetp, fnth, 0);
    pxoff = lxoff;
#endif

  }
  
  system("gpio write 4 1");
  system("gpio write 0 1");
  firlit = fvlit = 0;
  
  // log_action("snapshot end\n\n");

  cairo_destroy (cr);
  
  
  if (have_ip && !trupl)
  {  g_timeout_add_seconds(15, G_CALLBACK (force_gdbkp), NULL);
  }
  
  
  // if (tmlaps) g_timeout_add_seconds(tmlaps, G_CALLBACK (flash), NULL);
  return TRUE;
}



void video_start()
{   cam_on = 0;
    raspistill_end_misery("taking video");
    system("sudo rm /tmp/thv*.bmp");
    char cmdbuf[1024];
    raspicam_cmd_format(cmdbuf,0,1);
    printf("%s\n\n", cmdbuf);
    // system("/bin/bash /home/pi/escape.sh");
    system(cmdbuf);
    vid_on = 1;
}

void video_stop()
{   system("sudo pkill raspivid");

    GtkStyleContext *context = gtk_widget_get_style_context(recbtn);
    
gtk_style_context_remove_class(context, "redbk");

    char buffer[1024], filename[1024];
    FILE* pf = popen("date +\x25Y\x25m\x25\x64.\x25H\x25M\x25S.\x25N", "r");
    if (pf)
    {   int lines = 0;
        while (fgets(buffer, 1024, pf))
        {   if (buffer[0] == '2')           // NOT Y3K COMPLIANT
            {   strcpy(filename, buffer);
                int i;
                for (i=0; i<1023; i++)
                {   if (filename[i] <= ' ') 
                    {   filename[i] = 0;
                        i += 1024;
                    }
                }
                sprintf(buffer, "Timestamp: %s\n", filename);
                // log_action(buffer);
                goto _thing;
                break;
            }
        }
        _thing:
        ;
        fclose(pf);
    }
  

    
    char cmdbuf[1024];
    sprintf(cmdbuf, 
            "MP4Box -add /tmp/output.h264 /home/pi/Videos/%s.mp4",
            // "MP4Box -add /tmp/output.flv /home/pi/Videos/%s.mp4",
            filename
           );
    system(cmdbuf);
    
    sprintf(cmdbuf,
            "ffmpeg -framerate 4 -i /tmp/thv%%06d.bmp /home/pi/Videos/%s_h.mp4",
            filename
           );
    system(cmdbuf);
    
    
    vid_on = 0;
    cam_on = 1;
    raspistill_init();
}


/******************************************************************************/
/* EVENT HANDLERS                                                             */
/******************************************************************************/

/* Create a new surface of the appropriate size */
static gboolean
configure_event_cb (GtkWidget         *widget,
                    GdkEventConfigure *event,
                    gpointer           data)
{
  if (surface)
    cairo_surface_destroy (surface);

  surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                               CAIRO_CONTENT_COLOR,
                                               gtk_widget_get_allocated_width (widget),
                                               gtk_widget_get_allocated_height (widget)
                                              );

  /* Initialize the surface to black */
  clear_surface ();
  
  gwidget = widget;

  /* We've handled the configure event, no want for further processing. */
  return TRUE;
}

static gboolean
configure_eventp_cb (GtkWidget         *widget,
                     GdkEventConfigure *event,
                     gpointer           data)
{
  if (surfacep)
    cairo_surface_destroy (surfacep);

  surfacep = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                               CAIRO_CONTENT_COLOR,
                                               gtk_widget_get_allocated_width (widget),
                                               gtk_widget_get_allocated_height (widget)
                                              );

  /* Initialize the surface to black */
  clear_surfacep ();
  
  gwidgetp = widget;

  /* We've handled the configure event, no want for further processing. */
  return TRUE;
}

static gboolean
configure_eventph_cb (GtkWidget         *widget,
                      GdkEventConfigure *event,
                      gpointer           data)
{
  if (surfaceph)
    cairo_surface_destroy (surfaceph);

  surfaceph = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                               CAIRO_CONTENT_COLOR,
                                               gtk_widget_get_allocated_width (widget),
                                               gtk_widget_get_allocated_height (widget)
                                              );

  /* Initialize the surface to black */
  clear_surfaceph ();
  
  gwidgetph = widget;

  /* We've handled the configure event, no want for further processing. */
  return TRUE;
}

static gboolean
configure_slsh_view_cb (GtkWidget         *widget,
                      GdkEventConfigure *event,
                      gpointer           data)
{
  if (surfacesv)
    cairo_surface_destroy (surfacesv);

  surfacesv = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                               CAIRO_CONTENT_COLOR,
                                               gtk_widget_get_allocated_width (widget),
                                               gtk_widget_get_allocated_height (widget)
                                              );

  /* Initialize the surface to black */
  clear_surfacev ();

  /* We've handled the configure event, no want for further processing. */
  return TRUE;
}


/* Redraw the screen from the surface. Note that the ::draw
 * signal receives a ready-to-be-used cairo_t that is already
 * clipped to only draw the exposed areas of the widget
 */
static gboolean
draw_cb (GtkWidget *widget,
         cairo_t   *cr,
         gpointer   data)
{
  gcr = cr;
  if (!surface) return FALSE;
  
  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_paint (cr);

  return FALSE;
}



static gboolean
drawp_cb (GtkWidget *widget,
          cairo_t   *cr,
          gpointer   data)
{ 
  if (!GTK_IS_WIDGET (widget)) return FALSE;
  prevcr = cr;
  
  cairo_set_source_surface (cr, surfacep, 0, 0);
  gdk_cairo_set_source_pixbuf (cr, gpixp, pxoff, 0);
  cairo_paint (cr);
  
  printf("drawp_cb()\n");

  return FALSE;
}


static gboolean
drawph_cb (GtkWidget *widget,
           cairo_t   *cr,
           gpointer   data)
{
  if (!GTK_IS_WIDGET (widget)) return FALSE;
  prevhcr = cr;
  
  cairo_set_source_surface (cr, surfaceph, 0, 0);
  gdk_cairo_set_source_pixbuf (cr, gpixph, phxoff, 0);
  cairo_paint (cr);

  return FALSE;
}


static gboolean
drawsv_cb (GtkWidget *widget,
           cairo_t   *cr,
           gpointer   data)
{
  if (!GTK_IS_WIDGET (widget)) return FALSE;
  
  printf("Setting to surfacesv %i\n", surfacesv);
  cairo_set_source_surface (cr, surfacesv, 0, 0);
  printf("Setting to gpixsv %i\n", gpixsv);
  gdk_cairo_set_source_pixbuf (cr, gpixsv, shxoff, 0);
  cairo_paint (cr);

  return FALSE;
}


static void
close_window (void)
{   cam_on=0;
    raspistill_end_misery("closing main window");
  if (surface)
    cairo_surface_destroy (surface);
    surface = NULL;
}

void shutdown_screen();

static gboolean
motion_notify_event_cb (GtkWidget      *widget,
                        GdkEventMotion *event,
                        gpointer        data)
{
  /* paranoia check, in case we haven't gotten a configure event */
  if (surface == NULL)
    return FALSE;
    
    /*
  if (event->x && event->y && event->x < 16 && event->y < 16)
  {   char cmd[1024];
  
      shutdown_screen();
      
      system("sudo pkill raspistill");
      // system("sudo shutdown now");
      
      sprintf(cmd, "sudo kill %i", getpid());
      system(cmd);
      return FALSE;
  }*/

  // if (event->state & GDK_BUTTON1_MASK)
    flash();

  /* We've handled it, stop processing */
  return TRUE;
}



void chbtn_update(void)
{	/* gtk_button_set_label(chbtn, 
                       ch_mapping ? "NoIR: CIR"
                                 : "NoIR: RGI"
                      );
                      */
  if (!chbtn) return FALSE;
                      
  GtkStyleContext *context;
  context = gtk_widget_get_style_context(chbtn);
  switch (ch_mapping)
  {   case 0:
      gtk_style_context_remove_class(context, "cuv"); 
      gtk_style_context_remove_class(context, "veg");
      gtk_style_context_remove_class(context, "mono");
      gtk_style_context_add_class(context, "ugb");    
      gtk_style_context_remove_class(context, "raw"); 
      gtk_style_context_remove_class(context, "bee");
      gtk_button_set_label(chbtn, "Mode: UGB");
      break;
      
      case 5:
      gtk_style_context_remove_class(context, "cuv"); 
      gtk_style_context_remove_class(context, "veg");
      gtk_style_context_remove_class(context, "mono");
      gtk_style_context_add_class(context, "bee");    
      gtk_style_context_remove_class(context, "raw"); 
      gtk_button_set_label(chbtn, "Mode: Bee");
      break;
      
      case 2:
      gtk_style_context_remove_class(context, "cuv"); 
      gtk_style_context_remove_class(context, "veg");
      gtk_style_context_remove_class(context, "ugb");     
      gtk_style_context_add_class(context, "mono");    
      gtk_style_context_remove_class(context, "raw"); 
      gtk_button_set_label(chbtn, "Mode: Mono");
      gtk_style_context_remove_class(context, "bee");
      break;
      
      case 3:
      gtk_style_context_remove_class(context, "cuv"); 
      gtk_style_context_remove_class(context, "mono");
      gtk_style_context_remove_class(context, "ugb");     
      gtk_style_context_remove_class(context, "raw");     
      gtk_style_context_add_class(context, "veg");
      gtk_button_set_label(chbtn, "Mode: Veg");
      gtk_style_context_remove_class(context, "bee");
      break;
      
      case 4:
      gtk_style_context_remove_class(context, "cuv"); 
      gtk_style_context_remove_class(context, "veg");
      gtk_style_context_remove_class(context, "ugb");     
      gtk_style_context_remove_class(context, "mono");    
      gtk_style_context_add_class(context, "raw");
      gtk_style_context_remove_class(context, "bee");
      gtk_button_set_label(chbtn, "Mode: raw");
      break;
      
      case 1:
      default:
      gtk_style_context_remove_class(context, "ugb"); 
      gtk_style_context_remove_class(context, "veg");
      gtk_style_context_remove_class(context, "mono");    
      gtk_style_context_remove_class(context, "raw");  
      gtk_style_context_add_class(context, "cuv");
      gtk_style_context_remove_class(context, "bee");
      gtk_button_set_label(chbtn, "Mode: CUV");
  }
  gtk_widget_queue_draw(chbtn);
    gtk_widget_queue_draw(previewh_area);
    gtk_widget_queue_draw(preview_area);
    
    save_settings();
}


static gboolean chbtn_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{ /* ch_mapping++;
  if (ch_mapping == 3) ch_mapping++;
  if (ch_mapping > _MAX_NOIRTYPE) ch_mapping = 0;
  
  chbtn_update();
  save_settings(); */
  
  chmap_menu();
  
  return TRUE;
}


void
actually_shudown_computer(void)
{   cam_on=0;
    raspistill_end_misery("shutting down");
    usleep(531981);
    system("sudo pkill fbcp");
    usleep(531981);
    system("sudo shutdown now");
}


static gboolean xbtn_click(GtkWidget      *widget,
                           GdkEventButton *event,
                           gpointer        data)
{ if (event->x = lxbx && event->y == lxby) 
  { lxbx = event->x;
    lxby = event->y;
    return TRUE;
  }

  lxbx = event->x;
  lxby = event->y;
  
  char jlgsux[1024];
  sprintf(jlgsux, "xbtn_click ( %i, %i )", event->x, event->y);
  log_action(jlgsux);

  if (pwrr) shutdown_screen();
  else 
  {	pwrr = 17;
  	upd_btn_pwr();
  }
  return TRUE;
}
// exposure and resolution buttons: call raspistill_init();


static gboolean resmbtn_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{      if (camres <=  600) camres = 480;
  else if (camres <=  768) camres = 600;
  else if (camres <= 1080) camres = 768;
  
  upd_lbl_res();
  save_settings();
  raspistill_init();
  return TRUE;
}



static gboolean respbtn_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{      if (camres >=  768) camres = 1080;   // something is wrong with imgcomb
  else if (camres >=  600) camres =  768;
  else if (camres >=  480) camres =  600;
  
  upd_lbl_res();
  save_settings();
  raspistill_init();
  return TRUE;
}



static gboolean shutmbtn_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{ 
  burst--;
  if (burst<1) burst=1;
  
  upd_lbl_shut();
  save_settings();
  ctdn2camreinit = 13;
  // raspistill_init();
  return TRUE;
}

static gboolean shutpbtn_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{ 
  burst++;
  
  upd_lbl_shut();
  save_settings();
  ctdn2camreinit = 13;
  // raspistill_init();
  return TRUE;
}



static gboolean shutauto_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{ 
  burst = 1;
  save_settings();
  
  upd_lbl_shut();
  raspistill_init();
  return TRUE;
}


static gboolean explbl_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{   return TRUE;
    
    expmode++;
    if (expmode > 8) expmode = 0;
    
    save_settings();
    upd_lbl_exp();
    
    ctdn2camreinit = 13;
}


static gboolean flashon_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{ 
    expcomp-=3;
	if (expcomp < -10) expcomp = -10;
	raspistill_init();
    upd_lbl_expcomp();
  	save_settings();
}

static gboolean recbtn_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{ 
  if (vid_on) video_stop();
  else video_start();
  
  return TRUE;
}

static gboolean slsh_xbut_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{   
    gtk_window_present(window);
    gtk_window_close(slideshow);
    cam_on = 1;
    if (shutter > 999999) 
    {   shutter = -1;
        // upd_lbl_shut();
    }
    check_processes();
}

static gboolean exit_menu(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{   
    gtk_window_present(window);
    gtk_window_close(menu);
    cam_on = 1;
    if (shutter > 999999) 
    {   shutter = -1;
        // upd_lbl_shut();
    }
    check_processes();
}


static gboolean
btnugb_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{ 
    ch_mapping = 5;
    exit_menu(widget, event, data);
    chbtn_update();
}


static gboolean
btncuv_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{ 
    ch_mapping = 1;
    exit_menu(widget, event, data);
    chbtn_update();
}


static gboolean
btnmon_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{ 
    ch_mapping = 2;
    exit_menu(widget, event, data);
    chbtn_update();
}


static gboolean
btnraw_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{ 
    ch_mapping = 4;
    exit_menu(widget, event, data);
    chbtn_update();
}


void
delete_response (GtkDialog *dialog,
               gint       response_id,
               gpointer   user_data)
{   // printf("Response: %i\n", response_id);
    // system("sudo pkill raspistill");
    // kill(getpid(),SIGINT);
    
    if (response_id == -8)          // yes
    {   char cmdbuf[1024];
        if (!slsh_cimg[0]) return;
        
        sprintf(cmdbuf, "sudo rm %s", slsh_cimg);
        system(cmdbuf);
        slsh_view_file();
    }
}

static gboolean slsh_dbut_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{   GtkWidget * dlg = gtk_message_dialog_new (slideshow,
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_MESSAGE_WARNING,
                        GTK_BUTTONS_YES_NO,
                        "Really DELETE this photo?"
                        );
                        
    g_signal_connect(dlg, "response",
                      G_CALLBACK (delete_response), NULL);
    
    gtk_dialog_run (GTK_DIALOG (dlg));
    gtk_widget_destroy (dlg);
}

static gboolean slsh_lbut_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{   if (slsh_lsidx) slsh_lsidx--;
    else slsh_view_oldest();
    printf("slsh_lsidx = %i\n", slsh_lsidx);
    slsh_view_file();
}


void ifbtn_upd(void)
{   /* GtkStyleContext *context;
    context = gtk_widget_get_style_context(ifbtn);
    if (flashir)
    {   gtk_style_context_add_class(context, "ifon");
    }
    else
    {   gtk_style_context_remove_class(context, "ifon");
    }*/
}

static gboolean ifbtn_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{   expcomp+=3;
	if (expcomp > 10) expcomp = 10;
	raspistill_init();
    upd_lbl_expcomp();
  	save_settings();
}

void vfbtn_upd(void)
{	GtkStyleContext *context;
    context = gtk_widget_get_style_context(vfbtn);
    if (flashv)
    {   gtk_style_context_add_class(context, "vfon");
    }
    else
    {   gtk_style_context_remove_class(context, "vfon");
    }
}

static gboolean vfbtn_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{   expcomp=0;
	if (expcomp < -10) expcomp = -10;
	else
	{   raspistill_init();
	    upd_lbl_expcomp();
	}
  	save_settings();
}



static gboolean slsh_rbut_click(GtkWidget      *widget,
                           GdkEventMotion *event,
                           gpointer        data)
{   slsh_lsidx++;
    printf("slsh_lsidx = %i\n", slsh_lsidx);
    slsh_view_file();
}

int
window_key_pressed(GtkWidget *widget, GdkEventKey *key, gpointer user_data) 
{   
    if (key->keyval == GDK_KEY_Escape)
    {   /*system("sudo pkill unclutter");
        system("sudo kill \"$(< /tmp/czpid)\"");
        cam_on =0;
        raspistill_end_misery("exiting app");
        kill(getpid(),SIGINT);
        system("sudo pkill ctrlr");*/
        system("/bin/bash /home/pi/escape.sh");
        return TRUE;
    }
   
    if (key->keyval == GDK_KEY_space)
    {   flash();
        return TRUE;
    }
   
    if (key->keyval == GDK_KEY_BackSpace)
    {   slsh_xbut_click(slsh_xbut, NULL, NULL);
        return TRUE;
    }
   
    if (key->keyval == GDK_KEY_leftarrow)
    {   slsh_xbut_click(slsh_lbut, NULL, NULL);
        return TRUE;
    }
   
    if (key->keyval == GDK_KEY_rightarrow)
    {   slsh_xbut_click(slsh_rbut, NULL, NULL);
        return TRUE;
    }
    
    return FALSE;       // avoid swallow unknown keystrokes
}



/******************************************************************************/
/* SHIT THAT RUNS WHEN WE OPEN THE WINDOW LET SOME AIR IN THIS MOTHERFUCKER   */
/******************************************************************************/


void shutdown_screen()
{   GtkWidget *shitdeun = gtk_application_window_new (app);
        
    gtk_window_fullscreen(GTK_WINDOW(shitdeun));
    gtk_container_set_border_width (GTK_CONTAINER (shitdeun), 0);
    
    GtkWidget *shitlbl = 
        gtk_label_new(
            "Shutting down....\n\nWhen the green light turns OFF and stays off, it is\nsafe to flip the power switch."
                     );
                  
    gtk_container_add (GTK_CONTAINER (shitdeun), shitlbl); 
    
     
    gtk_widget_show_all(shitdeun); 
    
    cam_on=0;
    raspistill_end_misery("displaying shitdeun screen");
    
    g_timeout_add(2537, G_CALLBACK (actually_shudown_computer), NULL);
}


void open_slideshow(GtkWidget *widget, GdkEventKey *key, int user_data)
{
    
    GtkStyleContext *context;
    
    printf("Turning off camera.\n");
    cam_on = 0;
    slsh_lsidx = user_data;
    raspistill_end_misery("displaying slideshow");
  
    printf("Creating slideshow window.\n");
    slideshow = gtk_application_window_new (app);
    gtk_window_set_title (GTK_WINDOW (slideshow), "Slideshow");
    
    printf("Full-screening slideshow window.\n");
    gtk_window_fullscreen(GTK_WINDOW(slideshow));
    gtk_container_set_border_width (GTK_CONTAINER (slideshow), 0);
    
    printf("Connecting Esc keypress event.\n");
    g_signal_connect (slideshow, "key-press-event",
                    G_CALLBACK (window_key_pressed), NULL);

    
    // Big grid
    printf("Creating slideshow grid.\n");
    slsh_grid = gtk_grid_new();
    gtk_widget_set_size_request(slsh_grid, SCR_RES_X, SCR_RES_Y);
    gtk_container_add (GTK_CONTAINER (slideshow), slsh_grid);
    
    /*
    GtkWidget *slsh_lbut;
    GtkWidget *slsh_rbut;
    GtkWidget *slsh_view;
    GtkWidget *slsh_xbut;
    */
    
    int w, h;
    printf("Getting size.\n");
    gtk_window_get_size(slideshow, &w, &h);
    
    printf("Adding buttons.\n");
    slsh_lbut = gtk_button_new_with_label("<<");
    gtk_grid_attach(slsh_grid, slsh_lbut, 0, 0, 1, 1);
    
    slsh_rbut = gtk_button_new_with_label(">>");
    gtk_grid_attach(slsh_grid, slsh_rbut, 2, 0, 1, 1);
    
    slsh_dbut = gtk_button_new_with_label("DEL");
    gtk_grid_attach(slsh_grid, slsh_dbut, 3, 0, 1, 1);
    
    context = gtk_widget_get_style_context(slsh_dbut);
    gtk_style_context_add_class(context, "redbk");
    
    slsh_xbut = gtk_button_new_with_label("Back to Camera");
    gtk_grid_attach(slsh_grid, slsh_xbut, 1, 0, 1, 1);
    
    printf("Connecting button events.\n");
    g_signal_connect(slsh_xbut, "button-press-event",
                      G_CALLBACK (slsh_xbut_click), NULL);
    
    g_signal_connect(slsh_lbut, "button-press-event",
                      G_CALLBACK (slsh_lbut_click), NULL);
    
    g_signal_connect(slsh_rbut, "button-press-event",
                      G_CALLBACK (slsh_rbut_click), NULL);
    
    g_signal_connect(slsh_dbut, "button-press-event",
                      G_CALLBACK (slsh_dbut_click), NULL);
    
    slsh_view = gtk_drawing_area_new();
    g_signal_connect (slsh_view,"configure-event",
                    G_CALLBACK (configure_slsh_view_cb), NULL);

    printf("Sizing view.\n");
    gtk_widget_set_size_request(slsh_view, 
                                SCR_RES_X, 
                                SCR_RES_Y-81
                               );
                               
    printf("Connecting draw event.\n");
    g_signal_connect (slsh_view, "draw",
                      G_CALLBACK (drawsv_cb), NULL);
                               
    printf("Attaching grid.\n");
    gtk_grid_attach(slsh_grid, slsh_view, 0, 1, 4, 1);
   
    
    
    slsh_flbl = gtk_label_new("");
    gtk_grid_attach(slsh_grid, slsh_flbl, 0, 2, 4, 1);
    
    context = gtk_widget_get_style_context(slsh_flbl);
    gtk_style_context_add_class(context, "tinytxt");
    
    
    printf("Showing slideshow window.\n");
    gtk_widget_show_all(slideshow);
    printf("Displaying initial image file.\n");
    slsh_view_file();
}



void chmap_menu(GtkWidget *widget, GdkEventKey *key, int user_data)
{
    
    GtkStyleContext *context;
    
    printf("Turning off camera.\n");
    cam_on = 0;
    // slsh_lsidx = user_data;
    raspistill_end_misery("displaying channel map menu");
  
    printf("Creating menu window.\n");
    menu = gtk_application_window_new (app);
    gtk_window_set_title (GTK_WINDOW (menu), "Menu");
    
    printf("Full-screening menu window.\n");
    gtk_window_fullscreen(GTK_WINDOW(menu));
    gtk_container_set_border_width (GTK_CONTAINER (menu), 0);
    
    printf("Connecting Esc keypress event.\n");
    g_signal_connect (menu, "key-press-event",
                    G_CALLBACK (window_key_pressed), NULL);

    
    // Big grid
    printf("Creating menu grid.\n");
    menu_grid = gtk_grid_new();
    gtk_widget_set_size_request(menu_grid, SCR_RES_X, SCR_RES_Y);
    gtk_container_add (GTK_CONTAINER (menu), menu_grid);
    
    /*  GtkWidget *menu_btncuv;
        GtkWidget *menu_btnugb;
        GtkWidget *menu_btnmon;
        GtkWidget *menu_btnraw;
    */
    
    int w, h;
    printf("Getting size.\n");
    gtk_window_get_size(menu, &w, &h);
    
    printf("Adding buttons.\n");
    menu_btncuv = gtk_button_new_with_label("CUV");
    gtk_grid_attach(menu_grid, menu_btncuv, 0, 0, 1, 1);
    context = gtk_widget_get_style_context(menu_btncuv);
    gtk_style_context_add_class(context, "cuvbig");
    
    menu_btnugb = gtk_button_new_with_label("Bee");
    gtk_grid_attach(menu_grid, menu_btnugb, 1, 0, 1, 1);
    context = gtk_widget_get_style_context(menu_btnugb);
    gtk_style_context_add_class(context, "beebig");
    
    menu_btnmon = gtk_button_new_with_label("Mono");
    gtk_grid_attach(menu_grid, menu_btnmon, 0, 1, 1, 1);
    context = gtk_widget_get_style_context(menu_btnmon);
    gtk_style_context_add_class(context, "monbig");
    
    menu_btnraw = gtk_button_new_with_label("Raw");
    gtk_grid_attach(menu_grid, menu_btnraw, 1, 1, 1, 1);
    context = gtk_widget_get_style_context(menu_btnraw);
    gtk_style_context_add_class(context, "rawbig");
    
    
    gtk_widget_set_size_request(menu_btncuv, 
                                SCR_RES_X/2, 
                                SCR_RES_Y/2
                               );
    
    gtk_widget_set_size_request(menu_btnugb, 
                                SCR_RES_X/2, 
                                SCR_RES_Y/2
                               );
    
    gtk_widget_set_size_request(menu_btnmon, 
                                SCR_RES_X/2, 
                                SCR_RES_Y/2
                               );
                               
    gtk_widget_set_size_request(menu_btnraw, 
                                SCR_RES_X/2, 
                                SCR_RES_Y/2
                               );
    
    
    printf("Connecting button events.\n");
    g_signal_connect(menu_btncuv, "button-press-event",
                      G_CALLBACK (btncuv_click), NULL);
    
    g_signal_connect(menu_btnugb, "button-press-event",
                      G_CALLBACK (btnugb_click), NULL);
    
    g_signal_connect(menu_btnmon, "button-press-event",
                      G_CALLBACK (btnmon_click), NULL);
    
    g_signal_connect(menu_btnraw, "button-press-event",
                      G_CALLBACK (btnraw_click), NULL);
    
      
    
    printf("Showing slideshow window.\n");
    gtk_widget_show_all(menu);
}




static void
activate (GtkApplication* app,
          gpointer        user_data)
{
  GtkStyleContext *context;
  
  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Camera Control");
  
  
  system("unclutter -idle 0 &");
  
  
  g_signal_connect (window, "key-press-event",
                    G_CALLBACK (window_key_pressed), NULL);

  g_signal_connect (window, "destroy", G_CALLBACK (close_window), NULL);

  gtk_window_fullscreen(GTK_WINDOW(window));
  gtk_container_set_border_width (GTK_CONTAINER (window), 0);
  
  /*
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_widget_set_size_request(frame, SCR_RES_X, SCR_RES_Y);
  gtk_container_add (GTK_CONTAINER (window), frame);
  */
  
  grid = gtk_grid_new();
  GtkWidget *thipgrid = gtk_grid_new();
  
  
  
  // cell 1,0 thermal type
  gtk_grid_attach(grid, thipgrid, 1, 0, 1, 1);
  
  // thbtn = gtk_button_new_with_label(" ");
  // gtk_grid_attach(thipgrid, thbtn, 0, 0, 1, 1);
  
  
  gtk_widget_set_size_request(grid, SCR_RES_X, SCR_RES_Y);
  gtk_container_add (GTK_CONTAINER (window), grid);
  
  // cell 0,0 NoIR type
  chbtn = gtk_button_new_with_label("Mode: CUV");
  gtk_grid_attach(grid, chbtn, 0, 0, 1, 1);
  
  context = gtk_widget_get_style_context(chbtn);
  gtk_style_context_add_class(context, "cuv");
  
  gtk_widget_set_size_request(chbtn, 
                              SCR_RES_X/4, 
                              SCR_RES_Y/4
                              );
  
  g_signal_connect (chbtn, "button-press-event",
                    G_CALLBACK (chbtn_click), NULL);
  

  
  iplbl = gtk_label_new("IP Address");
  
  context = gtk_widget_get_style_context(iplbl);
  gtk_style_context_add_class(context, "tinytxt");
  gtk_grid_attach(thipgrid, iplbl, 0, 1, 1, 1);
  
  // cell 2,0 exit

  xbtn = gtk_button_new_with_label("");		// "OFF"
  gtk_grid_attach(grid, xbtn, 2, 0, 1, 1);
  
  context = gtk_widget_get_style_context(xbtn);
  gtk_style_context_add_class(context, "pwr");		// "redbk"
  
  gtk_widget_set_size_request(xbtn, 
                              SCR_RES_X/4, 
                              SCR_RES_Y/4
                              );
  
  g_signal_connect (xbtn, "button-press-event",
                    G_CALLBACK (xbtn_click), NULL);
  
  // cell 0,1 preview

  prevgrid = gtk_grid_new();
  gtk_grid_attach(grid, prevgrid, 0, 1, 1, 1);
  
  preview_area = gtk_drawing_area_new ();
  // preview_area = gtk_image_new ();
  gtk_widget_set_size_request(preview_area, 
                              SCR_RES_X/4, 
                              SCR_RES_Y/4
                              );
  gtk_grid_attach(prevgrid, preview_area, 0, 0, 1, 1);

  
  previewh_area = gtk_drawing_area_new ();
  // previewh_area = gtk_image_new ();
  gtk_widget_set_size_request(previewh_area, 
                              SCR_RES_X/4, 
                              SCR_RES_Y/4
                              );
  gtk_grid_attach(prevgrid, previewh_area, 0, 1, 1, 1);

  // cell 1,1 live image
  drawing_area = gtk_drawing_area_new ();
  gtk_widget_set_size_request(drawing_area, SCR_RES_X/2, SCR_RES_Y/2);
  gtk_grid_attach(grid, drawing_area, 1, 1, 1, 1);
  
  // cell 2,1 flash setting
  
  GtkWidget *flgrid = gtk_grid_new();
  gtk_grid_attach(grid, flgrid, 2, 1, 1, 1);
  
  ifbtn = gtk_button_new_with_label("Exp+");
  vfbtn = gtk_button_new_with_label("0");
  
  gtk_grid_attach(flgrid, ifbtn, 0, 0, 1, 1);
  gtk_grid_attach(flgrid, vfbtn, 0, 1, 1, 1);
  
  int ysz = allowvid
          ? SCR_RES_Y/6
          : SCR_RES_Y/4;
  
  gtk_widget_set_size_request(ifbtn, 
                              SCR_RES_X/4, 
                              ysz
                              );
                              
  gtk_widget_set_size_request(vfbtn, 
                              SCR_RES_X/4, 
                              ysz
                              );
  
  context = gtk_widget_get_style_context(ifbtn);
  // gtk_style_context_add_class(context, "ifon");
  
  g_signal_connect (ifbtn, "button-press-event",
                    G_CALLBACK (ifbtn_click), NULL);
  
  g_signal_connect (vfbtn, "button-press-event",
                    G_CALLBACK (vfbtn_click), NULL);
  
  
  // cell 0,2 resolution

  resgrid = gtk_grid_new();
  gtk_grid_attach(grid, resgrid, 0, 2, 1, 1);
  

  resmbtn = gtk_button_new_with_label("-");
  reslbl = gtk_label_new("800\nx600");
  respbtn = gtk_button_new_with_label("+");
  gtk_grid_attach(resgrid, reslbl, 1, 0, 1, 1);
  gtk_grid_attach_next_to(resgrid, resmbtn, reslbl, GTK_POS_LEFT, 1, 1);
  gtk_grid_attach_next_to(resgrid, respbtn, reslbl, GTK_POS_RIGHT, 1, 1);
  
  context = gtk_widget_get_style_context(reslbl);
  gtk_style_context_add_class(context, "smtxt");
  
  
  gtk_widget_set_size_request(reslbl, 
                              SCR_RES_X/8, 
                              SCR_RES_Y/4
                              );
                              
  gtk_widget_set_size_request(resmbtn, 
                              SCR_RES_X/16, 
                              SCR_RES_Y/4
                              );
                              
  gtk_widget_set_size_request(respbtn, 
                              SCR_RES_X/16, 
                              SCR_RES_Y/4
                              );
  
  g_signal_connect (resmbtn, "button-press-event",
                    G_CALLBACK (resmbtn_click), NULL);
  
  g_signal_connect (respbtn, "button-press-event",
                    G_CALLBACK (respbtn_click), NULL);
  
  
  // cell 1,2 shutter
  
  shexpgrid = gtk_grid_new();
  gtk_grid_attach(grid, shexpgrid, 1, 2, 1, 1);

  shutgrid = gtk_grid_new();
  gtk_grid_attach(shexpgrid, shutgrid, 0, 0, 1, 1);
  
  shutmbtn = gtk_button_new_with_label("-");
  
  // shutlbl = gtk_label_new("Shutter:\nAuto");
  shutlbl = gtk_button_new_with_label("Burst: 1");
  
  shutpbtn = gtk_button_new_with_label("+");
  gtk_grid_attach(shutgrid, shutlbl, 1, 0, 1, 1);
  gtk_grid_attach_next_to(shutgrid, shutmbtn, shutlbl, GTK_POS_LEFT, 1, 1);
  gtk_grid_attach_next_to(shutgrid, shutpbtn, shutlbl, GTK_POS_RIGHT, 1, 1);
  
  gtk_widget_set_size_request(shutlbl, 
                              SCR_RES_X/4, 
                              SCR_RES_Y/4
                              );
                              
  gtk_widget_set_size_request(shutmbtn, 
                              SCR_RES_X/8, 
                              SCR_RES_Y/8
                              );
                              
  gtk_widget_set_size_request(shutpbtn, 
                              SCR_RES_X/8, 
                              SCR_RES_Y/8
                              );
  
  g_signal_connect (shutmbtn, "button-press-event",
                    G_CALLBACK (shutmbtn_click), NULL);
  
  g_signal_connect (shutpbtn, "button-press-event",
                    G_CALLBACK (shutpbtn_click), NULL);
          
          
  /*                  
  explbl = gtk_button_new_with_label("Exp: auto"); 
  gtk_grid_attach(shexpgrid, explbl, 0, 1, 1, 1);             
  
  gtk_widget_set_size_request(explbl, 
                              SCR_RES_X/2, 
                              SCR_RES_Y/8
                              );
                              
  g_signal_connect (explbl, "button-press-event",
                    G_CALLBACK (explbl_click), NULL);                            
  */
  
  
  // cell 2,2 flash on
  // flonbtn = gtk_button_new_with_label("Auto\nShutter");       // shaut aup
  flonbtn = gtk_button_new_with_label("Exp-");
  ysz = allowvid
          ? SCR_RES_Y/6
          : SCR_RES_Y/4;
  
  gtk_widget_set_size_request(flonbtn, 
                              SCR_RES_X/4, 
                              ysz
                              );
  
  // cell 2,2 alternate: record btn               
  recbtn  = gtk_button_new_with_label("REC");

  if (allowvid)
  { // gtk_grid_attach(flgrid, flonbtn, 0, 2, 1, 1);
    gtk_grid_attach(grid, recbtn, 2, 2, 1, 1);
  }
  else
  {  gtk_grid_attach(grid, flonbtn, 2, 2, 1, 1);
  }
  
  g_signal_connect (shutlbl, "button-press-event",
                    G_CALLBACK (shutauto_click), NULL);
  
  g_signal_connect (flonbtn, "button-press-event",
                    G_CALLBACK (flashon_click), NULL);
                    
  g_signal_connect (recbtn, "button-press-event",
                    G_CALLBACK (recbtn_click), NULL);
                    
                    

  /* Signals used to handle the backing surface */
  g_signal_connect (drawing_area, "draw",
                    G_CALLBACK (draw_cb), NULL);
                    
  g_signal_connect (preview_area, "draw",
                    G_CALLBACK (drawp_cb), NULL);
                    
  g_signal_connect (previewh_area, "draw",
                    G_CALLBACK (drawph_cb), NULL);
                    
                    
  g_signal_connect (drawing_area,"configure-event",
                    G_CALLBACK (configure_event_cb), NULL);
                    
  g_signal_connect (preview_area,"configure-event",
                    G_CALLBACK (configure_eventp_cb), NULL);
                    
  g_signal_connect (previewh_area,"configure-event",
                    G_CALLBACK (configure_eventph_cb), NULL);
                    
  
  
  
  g_timeout_add(274, G_CALLBACK (thingdraw), NULL);
  check_processes();
  g_timeout_add(1000, G_CALLBACK (check_processes), NULL);
  g_timeout_add(1003, G_CALLBACK (redraw_btns), NULL);
  
  g_signal_connect (drawing_area, "button-press-event",
                    G_CALLBACK (motion_notify_event_cb), NULL);

  g_signal_connect (preview_area, "button-press-event",
                    G_CALLBACK (open_slideshow), -1);
                    
  g_signal_connect (previewh_area, "button-press-event",
                    G_CALLBACK (open_slideshow), -2);
                    
                    
                    
  gtk_widget_set_events (drawing_area, gtk_widget_get_events (drawing_area)
                                     | GDK_BUTTON_PRESS_MASK
                                     | GDK_POINTER_MOTION_MASK);
  
  gtk_widget_set_events (preview_area, gtk_widget_get_events (preview_area)
                                     | GDK_BUTTON_PRESS_MASK
                                     | GDK_POINTER_MOTION_MASK);

  gtk_widget_set_events (previewh_area, gtk_widget_get_events (previewh_area)
                                     | GDK_BUTTON_PRESS_MASK
                                     | GDK_POINTER_MOTION_MASK);
  
  gtk_widget_set_events (shutlbl, gtk_widget_get_events (shutlbl)
                                     | GDK_BUTTON_PRESS_MASK
                                     | GDK_POINTER_MOTION_MASK);
                                     
  gtk_widget_set_events (explbl, gtk_widget_get_events (explbl)
                                     | GDK_BUTTON_PRESS_MASK
                                     | GDK_POINTER_MOTION_MASK);
  
      /* styling background color to black */
    GtkCssProvider* provider = gtk_css_provider_new();
    GdkDisplay* display = gdk_display_get_default();
    GdkScreen* screen = gdk_display_get_default_screen(display);


    gtk_style_context_add_provider_for_screen(screen,
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER);
    
    gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(provider),
                                    global_css, 
                                    -1, NULL);
    g_object_unref(provider);
  
  
  
  
  
  
  gtk_widget_show_all (window);
	
	chbtn_update();
	upd_lbl_res();
	upd_lbl_shut();
	upd_lbl_exp();
	ifbtn_upd();
	vfbtn_upd();
  
  
}


void check_update(void)
{	/* if (!devbox) */ return 0;           // this dosent work & I dont want it on my dev cam
    if (have_ip) 
	{	gtk_label_set_text(iplbl, "Checking for updates...");
		system("~/swupd.sh");
	}
	else g_timeout_add_seconds(60, G_CALLBACK (check_update), NULL);
}



void save_settings(void)
{	FILE* pf = fopen("/home/pi/settings", "w");
	if (pf)
	{	fprintf(pf, "%i\n", ch_mapping);
		fprintf(pf, "%i\n", burst);
		fprintf(pf, "%i\n", camres);
		fprintf(pf, "%i\n", flashir);
		fprintf(pf, "%i\n", flashv);
		fprintf(pf, "%i\n", expmode);
		fclose(pf);
	}
}



void load_settings(void)
{	FILE* pf = fopen("/home/pi/settings", "r");
	if (pf)
	{	char buffer[1024];
		fgets(buffer, 1024, pf);	ch_mapping = atoi(buffer);
		fgets(buffer, 1024, pf);	burst     = atoi(buffer);
		fgets(buffer, 1024, pf);	camres    = atoi(buffer);
		fgets(buffer, 1024, pf);	flashir   = atoi(buffer);
		fgets(buffer, 1024, pf);	flashv    = atoi(buffer);
		fgets(buffer, 1024, pf);	expmode   = 7; // atoi(buffer);
		fclose(pf);
	}
	
	chbtn_update();
	upd_lbl_res();
	upd_lbl_shut();
	upd_lbl_exp();
	ifbtn_upd();
	vfbtn_upd();
}

int main (int argc, char **argv)
{   
    int ps = is_process_running("ctrlr");
    // printf("ps: %i\n", ps);
    
    if (ps > 1)
    {   cam_on=0;
        raspistill_end_misery("process already running");
        kill(getpid(),SIGINT);
        return 1;
    }
    
    if (cfileexists("/home/pi/devbox"))
    {   devbox=1;
        // allowvid=1;
    }
    
    int mypid = getpid();
    char cmdbuf[256];
    sprintf(cmdbuf, "sudo renice -n 5 %i", mypid);
    system(cmdbuf);
 
  cam_pid = 0;
  pwrr = 0;
  
  ch_mapping = 1;        // GBU
  shutter = -1;         // auto
  burst = 1;
  camres = 768;         // 1024x768px
  
  flashy = 0;           // not doing flash
  flashir = 1;          // default on
  flashv = 0;           // default off
  cam_on = 1;           // enable raspistill
  expmode = 7;          // antishake
  
  firlit = fvlit = 0;   // flash IR/visible lit
  
  thsamp = -1;
  
  listfirst = listlen = 0;
  
  load_settings();
  last_cam_init = 0;
  
  if (shutter > 999999) shutter = -1;
  
  int i;
  for (i=1; i<argc; i++)
  { if (!strcmp(argv[i], "tm"))        // time lapse
    {   tmlaps = 60;
        g_timeout_add_seconds(tmlaps, G_CALLBACK (flash), NULL);
    }
  }
  argc = 0;
  ctdn2camreinit = 0;
  
  system("gpio mode 4 out");
  system("gpio mode 0 out");
  system("gpio write 4 1");
  system("gpio write 0 1");
  
  system("sudo vcdbg set awb_mode 1");
  
#if _SIMULSHOT
  // if (!devbox)
#endif
  if (!is_process_running("czrun.sh"))
  { system("/bin/bash /home/pi/czrun.sh &");
  }
  
  check_disk_usage();
  
  // return 0;
  
  strcpy(slsh_cimg, "");
  
  int status;
	
  app = gtk_application_new ("org.gtk.example", G_APPLICATION_FLAGS_NONE);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);
  
  
  // g_timeout_add_seconds(30, G_CALLBACK (check_update), NULL);

  return status;
}


