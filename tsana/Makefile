EXECUTABLE = tsana.exe

all: $(EXECUTABLE)

include ../common.mak

$(EXECUTABLE): $(DEPS) $(OBJS) ../lib/list.o ../lib/url.o ../lib/udp.o
	$(CC) -o $(EXECUTABLE) $(OBJS) ../lib/list.o ../lib/url.o ../lib/udp.o -lwsock32
