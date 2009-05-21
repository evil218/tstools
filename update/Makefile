EXECUTABLE = update.exe

all: $(EXECUTABLE)

include ../common.mak

$(EXECUTABLE): $(DEPS) $(OBJS)
	$(CC) -o $(EXECUTABLE) $(OBJS) -lwsock32
