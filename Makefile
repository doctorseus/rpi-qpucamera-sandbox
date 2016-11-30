CC	=	gcc
CFLAGS	=	-c -Wall
LDFLAGS	=

GLCAM_PROGRAM	=	glcamera

GLCAM_INCLUDES	=	-I./\
					-I/opt/vc/include/

GLCAM_SOURCES	=	main.c RaspiCamControl.c RaspiCLI.c

#GLCAM_LDFLAGS	=	-L/opt/vc/lib/ -lmmal -lmmal_core -lmmal_util -lvcos -pthread -lbcm_host -lbrcmGLESv2 -lbrcmEGL -lm
GLCAM_LDFLAGS	=	-L/opt/vc/lib/ -lmmal -lmmal_core -lmmal_util -lvcos -lbcm_host

all: $(GLCAM_PROGRAM)

$(GLCAM_PROGRAM): clean
	$(CC) $(GLCAM_INCLUDES) $(GLCAM_SOURCES) $(GLCAM_LDFLAGS) -o $@

clean:
	rm -f $(GLCAM_PROGRAM)
