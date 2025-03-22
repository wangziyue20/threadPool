CC = gcc
CFLAGS = -Wall -g
TARGET = threadpool

all: $(TARGET)

$(TARGET): main.o threadpool.o
	$(CC) $(CFLAGS) -o $@ $^

main.o: main.c
	$(CC) $(CFLAGS) -c $< -o $@

threadpool.o: threadpool.c threadpool.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGET)