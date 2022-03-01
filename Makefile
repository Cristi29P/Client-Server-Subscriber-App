CC=g++ -Wall -Wextra

all: build

build: server subscriber

server: helpers.o server.o
	$(CC) $^ -o $@

server.o: server.cpp
	$(CC) $^ -c

subscriber: helpers.o subscriber.o
	$(CC) $^ -o $@

subscriber.o: subscriber.cpp
	$(CC) $^ -c

helpers.o: helpers.cpp
	$(CC) $^ -c

clean:
	rm *.o server subscriber