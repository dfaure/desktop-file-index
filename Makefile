all: compile tool

CFLAGS = `pkg-config --cflags --libs gtk+-3.0` -Wall -ggdb3
LDFLAGS = `pkg-config --libs gtk+-3.0`

consume: consume.o tool.o

tool: tool.o dfi-reader.o

clean:
	rm -f *.o compile tool index.cache
