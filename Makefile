CC	=	gcc
CFLAGS	=	-c -Wall
LDFLAGS	=

all: glcamera mcamera glscene

INCLUDES	=	-I./\
				-I/opt/vc/include/

CAM_SOURCES	=	RaspiCamControl.c RaspiCLI.c

#GLCAM_LDFLAGS	=	-L/opt/vc/lib/ -lmmal -lmmal_core -lmmal_util -lvcos -pthread -lbcm_host -lbrcmGLESv2 -lbrcmEGL -lm
LDFLAGS	=	-L/opt/vc/lib/ -lmmal -lmmal_core -lmmal_util -lvcos -lbcm_host -lbrcmEGL -lbrcmGLESv2

glcamera: clean
	$(CC) $(INCLUDES) $(CAM_SOURCES) $(LDFLAGS) glcamera.c -o $@

mcamera: clean
	$(CC) $(INCLUDES) $(CAM_SOURCES) $(LDFLAGS) mcamera.c -o $@

glscene: clean
	$(CC) $(INCLUDES) $(CAM_SOURCES) $(LDFLAGS) glscene.c -o $@

clean:
	rm -f glcamera
	rm -f mcamera
	rm -f glscene
