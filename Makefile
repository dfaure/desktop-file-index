all: consume.o compile.o

CFLAGS = `pkg-config --cflags --libs gtk+-3.0` -Wall -ggdb3
