CC = g++
CFLAGS = -Wall -g
all: server subscriber

server: server.cpp

subscriber: subscriber.cpp

clean:
	rm -f server subscriber
