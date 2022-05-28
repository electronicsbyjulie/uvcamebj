#define _XOPEN_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 

int main(char argc, char** argv)
{
	if (argc < 2)
	{	printf("Usage: readshm {id} {num_bytes} [{type}]\n");
		printf("Allowed types:\n\tc\tchar\n\tn\tint\n\tf\tfloat\n");
		return 0;
	}
	
	int id = atoi(argv[1]);
	int nb = atoi(argv[2]), nbm = nb;
	char dtd[8] = "char", *dtyp;
	if (argc > 2) dtyp = argv[3];
	else dtyp = dtd;

	switch (dtyp[0])
	{	case 'i':
		case 'n':
		nbm *= sizeof(int);
		break;
		
		case 'f':
		nbm *= sizeof(float);
		break;
		
		default:
		;
	}	

    int i;			// gcc is being a goddamn dildo not letting me loop initialize even with c11
	
	/*
	~/readshm 50 64 int
	4 arguments
	arg 0 = ./readshm
	arg 1 = 50
	arg 2 = 64
	arg 3 = int
	*/

	// printf("%i arguments\n", argc);
	// for (i=0; i<argc; i++) printf("arg %i = %s\n", i, argv[i]);
	int destid = -1;
	int dnb = nb * (nbm/nb);          // DNA, dnb, DNC, DnD, D&E
	if (argc > 4)
	{	if (!strcmp(argv[4], "cp"))
		{	destid = id+1;
			if (argc > 5) destid = atoi(argv[5]);
		}
	}
	
	
    key_t key = ftok("/tmp/shm",id);
    int shmid  = shmget(key, nbm, 0666 | IPC_CREAT);
    char *shmp = (char*)shmat(shmid, (void*)0, 0);
    int* shmpi = (int*)shmp;
    float* shmpf = (float*)shmp;
    
    
    if (destid >= 0)
    {   key_t keyd = ftok("/tmp/shm",destid);
		int shmidd  = shmget(keyd, nbm, 0666 | IPC_CREAT);
		char *shmpd = (char*)shmat(shmidd, (void*)0, 0);
		
		for (i=0; i<dnb; i++) shmpd[i] = shmp[i];
		
		return 0;
    }
    
    for (i=0; i<nb; i++) 
    {	
    	switch (dtyp[0])
    	{	case 'i':
    		case 'n':
    		if (!(i & 15)) printf("%X: ", i);
    		printf("%i ", shmpi[i]);
    		if ((i & 15) == 15) printf("\n");
    		break;
    		
    		case 'f':
    		if (!(i & 7)) printf("%X: ", i);
    		printf("%f ", shmpf[i]);
    		if ((i & 7) == 7) printf("\n");
    		break;
    		
    		default:
    		if (!(i & 7)) printf("%X: ", i);
    		printf("%02x ", shmp[i]);
	    	if ((i & 31) == 31) printf("\n");
		}
    }
    
    printf("\n");
    return 0;
}
