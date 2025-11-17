# Makefile for surinuke-gb (Asterisk Dodge Game)

CC = lcc
CFLAGS = -Wa-l -Wl-m -Wl-j

TARGET = surinuke-gb.gb
OBJS = main.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.c
	$(CC) -c -o main.o main.c

clean:
	rm -f *.o *.lst *.map *.sym $(TARGET)

.PHONY: all clean
