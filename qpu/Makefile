CC	=	gcc
CFLAGS	= -std=gnu99 -g
LDFLAGS	=

all: simple

INCLUDES	=	-I./\
				-I/opt/vc/include/\
				-I/opt/vc/src/hello_pi/hello_fft

SOURCES	=	/opt/vc/src/hello_pi/hello_fft/mailbox.c qpu.c

LDFLAGS	=	-L/opt/vc/lib/ -lbcm_host -ldl

simple: clean
	$(CC) $(INCLUDES) $(SOURCES) $(CFLAGS) $(LDFLAGS) simple.c -o $@

clean:
	rm -f simple
