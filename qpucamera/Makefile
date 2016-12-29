CC		= gcc
CFLAGS	= -std=gnu99 -g
LDFLAGS	=

all: qpucamera

INCLUDES	=	-I./\
				-I/opt/vc/include/

CAM_INCLUDES	= -I../camera/	
CAM_SOURCES		= ../camera/RaspiCamControl.c ../camera/RaspiCLI.c ../camera/tga.c

LDFLAGS			= -L/opt/vc/lib/ -lmmal -lmmal_core -lmmal_util -lvcos -lbcm_host -lbrcmEGL -lbrcmGLESv2 -lvcsm -ldl

qpucamera: clean asm
	$(CC) $(INCLUDES) $(CAM_INCLUDES) $(CAM_SOURCES) $(CFLAGS) $(LDFLAGS) -I/opt/vc/src/hello_pi/hello_fft /opt/vc/src/hello_pi/hello_fft/mailbox.c -I../qpu/ ../qpu/qpu.c mmal_camera.c qpucamera.c qpu_program.c -o $@

encodingtest: clean
	$(CC)  $(INCLUDES) $(CAM_INCLUDES) $(CAM_SOURCES) $(CFLAGS) $(LDFLAGS) -I/opt/vc/src/hello_pi/hello_fft /opt/vc/src/hello_pi/hello_fft/mailbox.c -I../qpu/ ../qpu/qpu.c mmal_camera.c mmal_video_encoding.c encodingtest.c -o $@

asm: qpucamera_kernel.asm
	vc4asm -V -o qpucamera_kernel.bin qpucamera_kernel.asm ../qpu/vc4.qinc
	vc4asm -V -o qpucamera_kernel_tmu.bin qpucamera_kernel_tmu.asm ../qpu/vc4.qinc
	vc4asm -V -o qpucamera_kernel_page.bin qpucamera_kernel_page.asm ../qpu/vc4.qinc

clean:
	rm -f qpucamera
	rm -f qpucamera_kernel.bin
