EXECUTABLE = test.exe

all: $(EXECUTABLE)

include ../common.mak

$(EXECUTABLE): $(DEPS) $(OBJS)
	$(CC) -o $(EXECUTABLE) $(OBJS) ../lib/crc.o
