CC		= cc
EXECUTABLE	= fusetest
MAIN		= main.c
INTERFACES	= fusefs.c log.c util.c
SRCS		= $(MAIN) $(INTERFACES)
OBJS		= $(SRCS:.c=.o)
LIBS		= `pkg-config --libs fuse libcurl`
LIBPATH		= -L/usr/local/lib
INCPATH		= -I/usr/local/include
CFLAGS		= -Wall -Ofast $(INCPATH) -g `pkg-config --cflags fuse libcurl`

##################################################################

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

$(EXECUTABLE) : $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBPATH) $(LIBS)

all : $(EXECUTABLE)

clean:
	rm -f *.o *.core $(EXECUTABLE)

depend:
	mkdep $(CFLAGS) $(SRCS)

