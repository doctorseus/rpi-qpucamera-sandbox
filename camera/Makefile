CC	=	gcc
CFLAGS	= -std=gnu99 -g
LDFLAGS	=

all: glcamera mcamera glscene

INCLUDES	=	-I./\
				-I/opt/vc/include/

CAM_SOURCES	=	RaspiCamControl.c RaspiCLI.c

#GLCAM_LDFLAGS	=	-L/opt/vc/lib/ -lmmal -lmmal_core -lmmal_util -lvcos -pthread -lbcm_host -lbrcmGLESv2 -lbrcmEGL -lm
LDFLAGS	=	-L/opt/vc/lib/ -lmmal -lmmal_core -lmmal_util -lvcos -lbcm_host -lbrcmEGL -lbrcmGLESv2 -lvcsm

glcamera: clean
	$(CC) $(INCLUDES) $(CAM_SOURCES) $(CFLAGS) $(LDFLAGS) glcamera.c -o $@

mcamera: clean
	$(CC) $(INCLUDES) $(CAM_SOURCES) $(CFLAGS) $(LDFLAGS) -I/opt/vc/src/hello_pi/hello_fft /opt/vc/src/hello_pi/hello_fft/mailbox.c mcamera.c -o $@

glscene: clean
	$(CC) $(INCLUDES) $(CAM_SOURCES) $(CFLAGS) $(LDFLAGS) glscene.c -o $@

clean:
	rm -f glcamera
	rm -f mcamera
	rm -f glscene
