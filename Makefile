all: compile tool

CFLAGS = `pkg-config --cflags --libs gtk+-3.0` -Wall -ggdb3 -Os
LDFLAGS = `pkg-config --libs gtk+-3.0`

consume: consume.o tool.o

tool: tool.o dfi-reader.o

compile: dfi-builder-string-table.o dfi-builder-keyfile.o dfi-builder-string-list.o dfi-builder-id-list.o dfi-builder-text-index.o compile.o

clean:
	rm -f *.o compile tool index.cache
