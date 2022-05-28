all: readshm ctrlr imgcomb 

doc_libvlc_gtk_player: doc_libvlc_gtk_player.c
	gcc -o gtk_player gtk_player.c `pkg-config --libs gtk+-3.0 libvlc` `pkg-config --cflags gtk+-3.0 libvlc`

ctrlr: ctrlr2.c globcss.h dispftns.c dispftns.h mkbmp.c
	# g++ -c dispftns.c -fpermissive -std=c++11
	gcc `pkg-config --cflags gtk+-3.0` -w -o ctrlr mkbmp.c ctrlr2.c dispftns.c `pkg-config --libs gtk+-3.0` -lm -Wint-conversion -Wincompatible-pointer-types                     

readshm: readshm.c
	gcc readshm.c -o readshm -w -lm -Wcpp -std=c11

imgcomb: imgcmbsm.c dispftns.c mkbmp.c
	gcc imgcmbsm.c dispftns.c mkbmp.c -o ~/imgcomb -w -ljpeg -lpng -lm -std=c99

