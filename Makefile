CC	=	gcc
CFLAGS	=	-c -Wall
LDFLAGS	=

all: glcamera glscene

GLCAM_INCLUDES	=	-I./\
					-I/opt/vc/include/

GLCAM_SOURCES	=	glcamera.c RaspiCamControl.c RaspiCLI.c

#GLCAM_LDFLAGS	=	-L/opt/vc/lib/ -lmmal -lmmal_core -lmmal_util -lvcos -pthread -lbcm_host -lbrcmGLESv2 -lbrcmEGL -lm
GLCAM_LDFLAGS	=	-L/opt/vc/lib/ -lmmal -lmmal_core -lmmal_util -lvcos -lbcm_host

glcamera: clean
	$(CC) $(GLCAM_INCLUDES) $(GLCAM_SOURCES) $(GLCAM_LDFLAGS) -o $@

glscene: clean
	$(CC) $(GLCAM_INCLUDES) $(GLCAM_LDFLAGS) -lbrcmEGL -lbrcmGLESv2 glscene.c -o $@

clean:
	rm -f glcamera
	rm -f glscene
